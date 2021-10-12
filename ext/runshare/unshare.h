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
};

int rb_unshare_internal(struct rb_unshare_args args);

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