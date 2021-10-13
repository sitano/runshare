#include <ruby.h>

#include "unshare.h"

#include "include/c.h"
#include "include/pwdutils.h"
#include "include/strutils.h"

/* we only need some defines missing in sys/mount.h, no libmount linkage */
#include <libmount/libmount.h>

static VALUE rb_mRUnshare;

enum {
    CLONE_NEWUSER,
    CLONE_NEWCGROUP,
    CLONE_NEWIPC,
    CLONE_NEWUTS,
    CLONE_NEWNET,
    CLONE_NEWPID,
    CLONE_NEWNS,
    CLONE_NEWTIME,
    FORK_ON_CLONE,
    WAIT_FORK,
    MOUNT_PROC,
    NEW_ROOT,
    NEW_DIR,
    MAP_ROOT_USER,
    MAP_CURRENT_USER,
    MAP_USER,
    MAP_GROUP,
    KEEP_CAPS,
    SET_UID,
    SET_GID,
    SET_GROUPS,
    PROPAGATION,
    FORCE_BOOTTIME,
    FORCE_MONOTONIC,
    KILL_CHILD,
    // list end marker
    FLAGS_COUNT
};

static ID id_clone_newuser;
static ID id_clone_newcgroup;
static ID id_clone_newipc;
static ID id_clone_newuts;
static ID id_clone_newnet;
static ID id_clone_newpid;
static ID id_clone_newns;
static ID id_clone_newtime;
static ID id_fork;
static ID id_wait;
static ID id_mount_proc;
static ID id_new_root;
static ID id_new_dir;
static ID id_map_root_user;
static ID id_map_current_user;
static ID id_map_user;
static ID id_map_group;
static ID id_keep_caps;
static ID id_set_uid;
static ID id_set_gid;
static ID id_set_groups;
static ID id_propagation;
static ID id_force_boottime;
static ID id_force_monotonic;
static ID id_kill_child;

static ID rb_unshare_keywords[FLAGS_COUNT];

static int
setgroups_str2id(const char *str) {
    size_t i;

    for (i = 0; i < ARRAY_SIZE(setgroups_strings); i++)
        if (strcmp(str, setgroups_strings[i]) == 0)
            return i;

    errx(EXIT_FAILURE, _("unsupported --setgroups argument '%s'"), str);
    return 0;
}

static unsigned long
parse_propagation(const char *str) {
    size_t i;
    static const struct prop_opts {
        const char *name;
        unsigned long flag;
    } opts[] = {
        {"slave",     MS_REC | MS_SLAVE},
        {"private",   MS_REC | MS_PRIVATE},
        {"shared",    MS_REC | MS_SHARED},
        {"unchanged", 0}
    };

    for (i = 0; i < ARRAY_SIZE(opts); i++) {
        if (strcmp(opts[i].name, str) == 0)
            return opts[i].flag;
    }

    errx(EXIT_FAILURE, _("unsupported propagation mode: %s"), str);

    return 0;
}

static void
ensure_string_ne(VALUE v, const char *err) {
    if (!RB_TYPE_P(v, RUBY_T_STRING) || (RSTRING_LEN(v)<1)) {
        rb_raise(rb_eArgError, err);
    }
}

static uid_t
get_user(const char *s, const char *err) {
    struct passwd *pw;
    char *buf = NULL;
    uid_t ret;

    pw = xgetpwnam(s, &buf);
    if (pw) {
        ret = pw->pw_uid;
        free(pw);
        free(buf);
    } else {
        ret = strtoul_or_err(s, err);
    }

    return ret;
}

static gid_t
get_group(const char *s, const char *err) {
    struct group *gr;
    char *buf = NULL;
    gid_t ret;

    gr = xgetgrnam(s, &buf);
    if (gr) {
        ret = gr->gr_gid;
        free(gr);
        free(buf);
    } else {
        ret = strtoul_or_err(s, err);
    }

    return ret;
}

static VALUE
rb_unshare(int argc, VALUE *argv, VALUE self) {
    VALUE opt = Qnil;

    struct rb_unshare_args args = {
        .set_groups = SETGROUPS_NONE,
        .map_user = -1,
        .map_group = -1,
        .propagation = UNSHARE_PROPAGATION_DEFAULT
    };

    rb_scan_args(argc, argv, "0:", &opt);

    if (!NIL_P(opt)) {
        VALUE kwvals[FLAGS_COUNT];

        rb_get_kwargs(opt, rb_unshare_keywords, 0, FLAGS_COUNT, kwvals);

        if (kwvals[CLONE_NEWUSER] != Qundef) args.clone_newuser = RTEST(kwvals[CLONE_NEWUSER]);
        if (kwvals[CLONE_NEWCGROUP] != Qundef) args.clone_newcgroup = RTEST(kwvals[CLONE_NEWCGROUP]);
        if (kwvals[CLONE_NEWIPC] != Qundef) args.clone_newipc = RTEST(kwvals[CLONE_NEWIPC]);
        if (kwvals[CLONE_NEWUTS] != Qundef) args.clone_newuts = RTEST(kwvals[CLONE_NEWUTS]);
        if (kwvals[CLONE_NEWNET] != Qundef) args.clone_newnet = RTEST(kwvals[CLONE_NEWNET]);
        if (kwvals[CLONE_NEWPID] != Qundef) args.clone_newpid = RTEST(kwvals[CLONE_NEWPID]);
        if (kwvals[CLONE_NEWNS] != Qundef) args.clone_newns = RTEST(kwvals[CLONE_NEWNS]);
        if (kwvals[CLONE_NEWTIME] != Qundef) args.clone_newtime = RTEST(kwvals[CLONE_NEWTIME]);
        if (kwvals[FORK_ON_CLONE] != Qundef) args.fork = RTEST(kwvals[FORK_ON_CLONE]);
        if (kwvals[WAIT_FORK] != Qundef) args.wait = RTEST(kwvals[WAIT_FORK]);
        args.mount_proc = kwvals[MOUNT_PROC];
        if (args.mount_proc != Qundef) ensure_string_ne(args.mount_proc, "invalid mount type");
        args.root = kwvals[NEW_ROOT];
        if (args.root != Qundef) ensure_string_ne(args.root, "invalid root type");
        args.new_dir = kwvals[NEW_DIR];
        if (args.new_dir != Qundef) ensure_string_ne(args.new_dir, "invalid new dir type");
        if (kwvals[MAP_ROOT_USER] != Qundef) args.map_root_user = RTEST(kwvals[MAP_ROOT_USER]);
        if (kwvals[MAP_CURRENT_USER] != Qundef) args.map_current_user = RTEST(kwvals[MAP_CURRENT_USER]);
        if (kwvals[MAP_USER] != Qundef) {
            args.map_user = get_user(StringValueCStr(kwvals[MAP_USER]), _("failed to parse uid"));
        }
        if (kwvals[MAP_GROUP] != Qundef) {
            args.map_group = get_group(StringValueCStr(kwvals[MAP_GROUP]), _("failed to parse gid"));
        }
        if (kwvals[KEEP_CAPS] != Qundef) args.keep_caps = RTEST(kwvals[KEEP_CAPS]);
        if (kwvals[SET_UID] != Qundef) args.set_uid = NUM2UIDT(kwvals[SET_UID]);
        if (kwvals[SET_GID] != Qundef) args.set_gid = NUM2GIDT(kwvals[SET_GID]);
        if (kwvals[SET_GROUPS] != Qundef) args.set_groups = setgroups_str2id(StringValueCStr(kwvals[SET_GROUPS]));
        if (kwvals[PROPAGATION] != Qundef) args.propagation = parse_propagation(StringValueCStr(kwvals[PROPAGATION]));
        if (kwvals[FORCE_BOOTTIME] != Qundef) args.force_boottime = RTEST(kwvals[FORCE_BOOTTIME]);
        if (kwvals[FORCE_MONOTONIC] != Qundef) args.force_monotonic = RTEST(kwvals[FORCE_MONOTONIC]);
        if (kwvals[KILL_CHILD] != Qundef) args.kill_child = RTEST(kwvals[KILL_CHILD]);
    }

    return INT2FIX(rb_unshare_internal(args));
}

void
Init_runshare(void) {
    rb_mRUnshare = rb_define_module("RUnshare");
    rb_define_singleton_method(rb_mRUnshare, "unshare", rb_unshare, -1);

    id_clone_newuser = rb_intern("clone_newuser");
    id_clone_newcgroup = rb_intern("clone_newcgroup");
    id_clone_newipc = rb_intern("clone_newipc");
    id_clone_newuts = rb_intern("clone_newuts");
    id_clone_newnet = rb_intern("clone_newnet");
    id_clone_newpid = rb_intern("clone_newpid");
    id_clone_newns = rb_intern("clone_newns");
    id_clone_newtime = rb_intern("clone_newtime");
    id_fork = rb_intern("fork");
    id_wait = rb_intern("wait");
    id_mount_proc = rb_intern("mount_proc");
    id_new_root = rb_intern("root");
    id_new_dir = rb_intern("new_dir");
    id_map_root_user = rb_intern("map_root_user");
    id_map_current_user = rb_intern("map_current_user");
    id_map_user = rb_intern("map_user");
    id_map_group = rb_intern("map_group");
    id_keep_caps = rb_intern("keep_caps");
    id_set_uid = rb_intern("set_uid");
    id_set_gid = rb_intern("set_gid");
    id_set_groups = rb_intern("set_groups");
    id_propagation = rb_intern("propagation");
    id_force_boottime = rb_intern("force_boottime");
    id_force_monotonic = rb_intern("force_monotonic");
    id_kill_child = rb_intern("kill_child");

    rb_unshare_keywords[CLONE_NEWUSER] = id_clone_newuser;
    rb_unshare_keywords[CLONE_NEWCGROUP] = id_clone_newcgroup;
    rb_unshare_keywords[CLONE_NEWIPC] = id_clone_newipc;
    rb_unshare_keywords[CLONE_NEWUTS] = id_clone_newuts;
    rb_unshare_keywords[CLONE_NEWNET] = id_clone_newnet;
    rb_unshare_keywords[CLONE_NEWPID] = id_clone_newpid;
    rb_unshare_keywords[CLONE_NEWNS] = id_clone_newns;
    rb_unshare_keywords[CLONE_NEWTIME] = id_clone_newtime;
    rb_unshare_keywords[FORK_ON_CLONE] = id_fork;
    rb_unshare_keywords[WAIT_FORK] = id_wait;
    rb_unshare_keywords[MOUNT_PROC] = id_mount_proc;
    rb_unshare_keywords[NEW_ROOT] = id_new_root;
    rb_unshare_keywords[NEW_DIR] = id_new_dir;
    rb_unshare_keywords[MAP_ROOT_USER] = id_map_root_user;
    rb_unshare_keywords[MAP_CURRENT_USER] = id_map_current_user;
    rb_unshare_keywords[MAP_USER] = id_map_user;
    rb_unshare_keywords[MAP_GROUP] = id_map_group;
    rb_unshare_keywords[KEEP_CAPS] = id_keep_caps;
    rb_unshare_keywords[SET_UID] = id_set_uid;
    rb_unshare_keywords[SET_GID] = id_set_gid;
    rb_unshare_keywords[SET_GROUPS] = id_set_groups;
    rb_unshare_keywords[PROPAGATION] = id_propagation;
    rb_unshare_keywords[FORCE_BOOTTIME] = id_force_boottime;
    rb_unshare_keywords[FORCE_MONOTONIC] = id_force_monotonic;
    rb_unshare_keywords[KILL_CHILD] = id_kill_child;
}