/*
 * unshare(1) - command-line interface for unshare(2)
 *
 * Copyright (C) 2009 Mikhail Gusarov <dottedmag@dottedmag.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <grp.h>
#include <ruby.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "include/c.h"
#include "include/caputils.h"
#include "include/namespace.h"
#include "include/pathnames.h"
#include "include/all-io.h"

#include "unshare.h"

/* /proc namespace files and mountpoints for binds */
static struct namespace_file {
    int		type;		/* CLONE_NEW* */
    const char	*name;		/* ns/<type> */
    const char	*target;	/* user specified target for bind mount */
} namespace_files[] = {
    { .type = CLONE_NEWUSER,  .name = "ns/user" },
    { .type = CLONE_NEWCGROUP,.name = "ns/cgroup" },
    { .type = CLONE_NEWIPC,   .name = "ns/ipc"  },
    { .type = CLONE_NEWUTS,   .name = "ns/uts"  },
    { .type = CLONE_NEWNET,   .name = "ns/net"  },
    { .type = CLONE_NEWPID,   .name = "ns/pid_for_children" },
    { .type = CLONE_NEWNS,    .name = "ns/mnt"  },
    { .type = CLONE_NEWTIME,  .name = "ns/time_for_children" },
    { .name = NULL }
};

static int npersists = 0;	/* number of persistent namespaces */

static void setgroups_control(int action)
{
    const char *file = _PATH_PROC_SETGROUPS;
    const char *cmd;
    int fd;

    if (action < 0 || (size_t) action >= ARRAY_SIZE(setgroups_strings))
        return;
    cmd = setgroups_strings[action];

    fd = open(file, O_WRONLY);
    if (fd < 0) {
        if (errno == ENOENT)
            return;
        err(EXIT_FAILURE, _("cannot open %s"), file);
    }

    if (write_all(fd, cmd, strlen(cmd)))
        err(EXIT_FAILURE, _("write failed %s"), file);
    close(fd);
}

static void map_id(const char *file, uint32_t from, uint32_t to)
{
    char *buf;
    int fd;

    fd = open(file, O_WRONLY);
    if (fd < 0)
        err(EXIT_FAILURE, _("cannot open %s"), file);

    xasprintf(&buf, "%u %u 1", from, to);
    if (write_all(fd, buf, strlen(buf)))
        err(EXIT_FAILURE, _("write failed %s"), file);
    free(buf);
    close(fd);
}

static void set_propagation(unsigned long flags)
{
    if (flags == 0)
        return;

    if (mount("none", "/", NULL, flags, NULL) != 0)
        err(EXIT_FAILURE, _("cannot change root filesystem propagation"));
}

static int set_ns_target(int type, const char *path)
{
    struct namespace_file *ns;

    for (ns = namespace_files; ns->name; ns++) {
        if (ns->type != type)
            continue;
        ns->target = path;
        npersists++;
        return 0;
    }

    return -EINVAL;
}

static int bind_ns_files(pid_t pid)
{
    struct namespace_file *ns;
    char src[PATH_MAX];

    for (ns = namespace_files; ns->name; ns++) {
        if (!ns->target)
            continue;

        snprintf(src, sizeof(src), "/proc/%u/%s", (unsigned) pid, ns->name);

        if (mount(src, ns->target, NULL, MS_BIND, NULL) != 0)
            err(EXIT_FAILURE, _("mount %s on %s failed"), src, ns->target);
    }

    return 0;
}

static ino_t get_mnt_ino(pid_t pid)
{
    struct stat st;
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "/proc/%u/ns/mnt", (unsigned) pid);

    if (stat(path, &st) != 0)
        err(EXIT_FAILURE, _("stat of %s failed"), path);
    return st.st_ino;
}

static void settime(time_t offset, clockid_t clk_id)
{
    char buf[sizeof(stringify_value(ULONG_MAX)) * 3];
    int fd, len;

    len = snprintf(buf, sizeof(buf), "%d %ld 0", clk_id, offset);

    fd = open("/proc/self/timens_offsets", O_WRONLY);
    if (fd < 0)
        err(EXIT_FAILURE, _("failed to open /proc/self/timens_offsets"));

    if (write(fd, buf, len) != len)
        err(EXIT_FAILURE, _("failed to write to /proc/self/timens_offsets"));

    close(fd);
}

static void bind_ns_files_from_child(pid_t *child, int fds[2])
{
    char ch;
    pid_t ppid = getpid();
    ino_t ino = get_mnt_ino(ppid);

    if (pipe(fds) < 0)
        err(EXIT_FAILURE, _("pipe failed"));

    *child = fork();

    switch (*child) {
        case -1:
            err(EXIT_FAILURE, _("fork failed"));

        case 0:	/* child */
            close(fds[1]);
            fds[1] = -1;

            /* wait for parent */
            if (read_all(fds[0], &ch, 1) != 1 && ch != PIPE_SYNC_BYTE)
                err(EXIT_FAILURE, _("failed to read pipe"));
            if (get_mnt_ino(ppid) == ino)
                exit(EXIT_FAILURE);
            bind_ns_files(ppid);
            exit(EXIT_SUCCESS);
            break;

        default: /* parent */
            close(fds[0]);
            fds[0] = -1;
            break;
    }
}

int rb_unshare_internal(struct rb_unshare_args args)
{
    int unshare_flags = 0;

    // int kill_child_signo = 0; /* 0 means --kill-child was not used */
    char *procmnt = NULL;
    char *newroot = NULL;
    char *newdir = NULL;

    int fds[2];
    int status;

    int pid_bind = 0;
    int pid = 0;

    time_t monotonic = 0;
    time_t boottime = 0;

    uid_t real_euid = geteuid();
    gid_t real_egid = getegid();

    if (args.clone_newns) {
        unshare_flags |= CLONE_NEWNS;
        // if (optarg)
        //    set_ns_target(CLONE_NEWNS, optarg);
    }
    if (args.clone_newuts) {
        unshare_flags |= CLONE_NEWUTS;
        // if (optarg)
        //    set_ns_target(CLONE_NEWUTS, optarg);
    }
    if (args.clone_newipc) {
        unshare_flags |= CLONE_NEWIPC;
        // if (optarg)
        //    set_ns_target(CLONE_NEWIPC, optarg);
    }
    if (args.clone_newnet) {
        unshare_flags |= CLONE_NEWNET;
        // if (optarg)
        //    set_ns_target(CLONE_NEWNET, optarg);
    }
    if (args.clone_newpid) {
        unshare_flags |= CLONE_NEWPID;
        // if (optarg)
        //    set_ns_target(CLONE_NEWPID, optarg);
    }
    if (args.clone_newuser) {
        unshare_flags |= CLONE_NEWUSER;
        // if (optarg)
        //    set_ns_target(CLONE_NEWUSER, optarg);
    }
    if (args.clone_newcgroup) {
        unshare_flags |= CLONE_NEWCGROUP;
        // if (optarg)
        //    set_ns_target(CLONE_NEWCGROUP, optarg);
    }
    if (args.clone_newtime) {
        unshare_flags |= CLONE_NEWTIME;
        // if (optarg)
        //    set_ns_target(CLONE_NEWTIME, optarg);
    }
    if (args.mount_proc != Qundef) {
        unshare_flags |= CLONE_NEWNS;
        procmnt = StringValueCStr(args.mount_proc);
    }
    if (args.map_user != (uid_t) -1) {
        unshare_flags |= CLONE_NEWUSER;
    }
    if (args.map_group != (gid_t) -1) {
        unshare_flags |= CLONE_NEWUSER;
    }
    if (args.map_root_user) {
        unshare_flags |= CLONE_NEWUSER;
        args.map_user = 0;
        args.map_group = 0;
    }
    if (args.map_current_user) {
        unshare_flags |= CLONE_NEWUSER;
        args.map_user = real_euid;
        args.map_group = real_egid;
    }
    if (args.kill_child) {
        args.fork = true;
        //     if (optarg) {
        //         if ((kill_child_signo = signame_to_signum(optarg)) < 0)
        //             errx(EXIT_FAILURE, _("unknown signal: %s"),
        //                  optarg);
        //     } else {
        //         kill_child_signo = SIGKILL;
        //     }
    }
    if (args.keep_caps) {
        cap_last_cap(); /* Force last cap to be cached before we fork. */
    }
    if (args.root != Qundef) {
        newroot = StringValueCStr(args.root);
    }
    // case OPT_MONOTONIC:
    //     monotonic = strtoul_or_err(optarg, _("failed to parse monotonic offset"));
    //     force_monotonic = 1;
    //     break;
    // case OPT_BOOTTIME:
    //     boottime = strtoul_or_err(optarg, _("failed to parse boottime offset"));
    //     force_boottime = 1;
    //     break;

    if ((args.force_monotonic || args.force_boottime) && !(unshare_flags & CLONE_NEWTIME))
        errx(EXIT_FAILURE, _("options --monotonic and --boottime require "
                             "unsharing of a time namespace (-t)"));

    if (npersists && (unshare_flags & CLONE_NEWNS))
        bind_ns_files_from_child(&pid_bind, fds);

    if (-1 == unshare(unshare_flags))
        err(EXIT_FAILURE, _("unshare failed"));

    if (args.force_boottime)
        settime(boottime, CLOCK_BOOTTIME);

    if (args.force_monotonic)
        settime(monotonic, CLOCK_MONOTONIC);

    if (args.fork) {
        /* force child forking before mountspace binding
         * so pid_for_children is populated.
         * Silence:
         *      warning: pthread_create failed for timer: Invalid argument, scheduling broken
         * by setting $VERBOSE = nil.
         * */
        VALUE res = rb_eval_string("Process.fork");
        pid = NIL_P(res) ? 0 : NUM2INT(res);

        switch(pid) {
            case -1:
                err(EXIT_FAILURE, _("fork failed"));
            case 0:	/* child */
                if (pid_bind && (unshare_flags & CLONE_NEWNS))
                    close(fds[1]);
                break;
            default: /* parent */
                break;
        }
    }

    if (npersists && (pid || !args.fork)) {
        /* run in parent */
        if (pid_bind && (unshare_flags & CLONE_NEWNS)) {
            int rc;
            char ch = PIPE_SYNC_BYTE;

            /* signal child we are ready */
            write_all(fds[1], &ch, 1);
            close(fds[1]);
            fds[1] = -1;

            /* wait for bind_ns_files_from_child() */
            do {
                rc = NUM2INT(PIDT2NUM(rb_waitpid(pid_bind, &status, 0)));
                if (rc < 0) {
                    if (errno == EINTR)
                        continue;
                    rb_sys_fail("rb_waitpid");
                }
                if (WIFEXITED(status) &&
                    WEXITSTATUS(status) != EXIT_SUCCESS) {
                    return NUM2PIDT(INT2NUM(pid));
                }
            } while (rc < 0);
        } else {
            /* simple way, just bind */
            bind_ns_files(getpid());
        }
    }

    if (pid) {
        if (!args.wait) {
            return NUM2PIDT(INT2NUM(pid));
        }

        if (NUM2INT(PIDT2NUM(rb_waitpid(pid, &status, 0))) == -1) {
            rb_sys_fail("rb_waitpid");
        }

        if (WIFEXITED(status)) {
            return NUM2PIDT(INT2NUM(pid));
        }

        if (WIFSIGNALED(status)) {
            kill(getpid(), WTERMSIG(status));
        }

        err(EXIT_FAILURE, _("child exit failed"));
    }

    if (args.kill_child) {
        // if (kill_child_signo != 0 && prctl(PR_SET_PDEATHSIG, kill_child_signo) < 0)
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0)
            err(EXIT_FAILURE, "prctl failed");
    }

    if (args.map_user != (uid_t) -1)
        map_id(_PATH_PROC_UIDMAP, args.map_user, real_euid);

    /* Since Linux 3.19 unprivileged writing of /proc/self/gid_map
     * has been disabled unless /proc/self/setgroups is written
     * first to permanently disable the ability to call setgroups
     * in that user namespace. */
    if (args.map_group != (gid_t) -1) {
        if (args.set_groups == SETGROUPS_ALLOW)
            errx(EXIT_FAILURE, _("options --setgroups=allow and "
                                 "--map-group are mutually exclusive"));
        setgroups_control(SETGROUPS_DENY);
        map_id(_PATH_PROC_GIDMAP, args.map_group, real_egid);
    }

    if (args.set_groups != SETGROUPS_NONE)
        setgroups_control(args.set_groups);

    if ((unshare_flags & CLONE_NEWNS) && args.propagation)
        set_propagation(args.propagation);

    if (newroot) {
        if (chroot(newroot) != 0)
            err(EXIT_FAILURE,
                _("cannot change root directory to '%s'"), newroot);
        newdir = newdir ?: "/";
    }
    if (newdir && chdir(newdir))
        err(EXIT_FAILURE, _("cannot chdir to '%s'"), newdir);

    if (procmnt) {
        /* When not changing root and using the default propagation flags
           then the recursive propagation change of root will
           automatically change that of an existing proc mount. */
        if (!newroot && args.propagation != (MS_PRIVATE|MS_REC)) {
            int rc = mount("none", procmnt, NULL, MS_PRIVATE|MS_REC, NULL);

            /* Custom procmnt means that proc is very likely not mounted, causing EINVAL.
               Ignoring the error in this specific instance is considered safe. */
            if(rc != 0 && errno != EINVAL)
                err(EXIT_FAILURE, _("cannot change %s filesystem propagation"), procmnt);
        }

        if (mount("proc", procmnt, "proc", MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL) != 0)
            err(EXIT_FAILURE, _("mount %s failed"), procmnt);
    }

    if (args.set_gid) {
        if (setgroups(0, NULL) != 0)	/* drop supplementary groups */
            err(EXIT_FAILURE, _("setgroups failed"));
        if (setgid(args.set_gid) < 0)		/* change GID */
            err(EXIT_FAILURE, _("setgid failed"));
    }
    if (args.set_uid && setuid(args.set_uid) < 0)	/* change UID */
        err(EXIT_FAILURE, _("setuid failed"));

    /* We use capabilities system calls to propagate the permitted
     * capabilities into the ambient set because we have already
     * forked so are in async-signal-safe context. */
    if (args.keep_caps && (unshare_flags & CLONE_NEWUSER)) {
        struct __user_cap_header_struct header = {
            .version = _LINUX_CAPABILITY_VERSION_3,
            .pid = 0,
        };

        struct __user_cap_data_struct payload[_LINUX_CAPABILITY_U32S_3] = {{ 0 }};
        uint64_t effective, cap;

        if (capget(&header, payload) < 0)
            err(EXIT_FAILURE, _("capget failed"));

        /* In order the make capabilities ambient, we first need to ensure
         * that they are all inheritable. */
        payload[0].inheritable = payload[0].permitted;
        payload[1].inheritable = payload[1].permitted;

        if (capset(&header, payload) < 0)
            err(EXIT_FAILURE, _("capset failed"));

        effective = ((uint64_t)payload[1].effective << 32) |  (uint64_t)payload[0].effective;

        for (cap = 0; cap < (sizeof(effective) * 8); cap++) {
            /* This is the same check as cap_valid(), but using
             * the runtime value for the last valid cap. */
            if (cap > (uint64_t) cap_last_cap())
                continue;

            if ((effective & (1 << cap))
                && prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) < 0)
                err(EXIT_FAILURE, _("prctl(PR_CAP_AMBIENT) failed"));
        }
    }

    return NUM2PIDT(INT2NUM(pid));
}
