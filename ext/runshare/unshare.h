#ifndef UNSHARE_H
#define UNSHARE_H 1

#include <stdbool.h>
#include <stdio.h>

#include "include/c.h"

#undef _
# define _(Text) (Text)

struct rb_unshare_args {
    bool clone_newuser;
    bool clone_newcgroup;
    bool clone_newipc;
    bool clone_newuts;
    bool clone_newnet;
    bool clone_newpid;
    bool clone_newns;
    bool clone_newtime;

    bool fork;
    bool wait;

    // default: /proc
    VALUE mount_proc;
    VALUE root;

    VALUE new_dir;
    bool map_root_user;
    bool map_current_user;
    uid_t map_user;
    gid_t map_group;
    bool keep_caps;
    uid_t set_uid;
    gid_t set_gid;
    int set_groups;
    unsigned long propagation;
    bool force_boottime;
    bool force_monotonic;
    bool kill_child;
};

int rb_unshare_internal(struct rb_unshare_args args);

/* synchronize parent and child by pipe */
#define PIPE_SYNC_BYTE	0x06

/* 'private' is kernel default */
#define UNSHARE_PROPAGATION_DEFAULT	(MS_REC | MS_PRIVATE)

enum {
    SETGROUPS_NONE = -1,
    SETGROUPS_DENY = 0,
    SETGROUPS_ALLOW = 1,
};

static const char *setgroups_strings[] =
    {
        [SETGROUPS_DENY] = "deny",
        [SETGROUPS_ALLOW] = "allow"
    };

#ifndef XALLOC_EXIT_CODE
# define XALLOC_EXIT_CODE EXIT_FAILURE
#endif

static inline
__attribute__((__format__(printf, 2, 3)))
int xasprintf(char **strp, const char *fmt, ...) {
    int ret;
    va_list args;

    va_start(args, fmt);
    ret = vasprintf(&(*strp), fmt, args);
    va_end(args);
    if (ret < 0)
        err(XALLOC_EXIT_CODE, "cannot allocate string");
    return ret;
}

#endif