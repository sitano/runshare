#ifndef UNSHARE_H
#define UNSHARE_H 1

#include <stdbool.h>

struct rb_unshare_args {
  bool clone_newuser;
  bool clone_newcgroup;
  bool clone_newipc;
  bool clone_newuts;
  bool clone_newnet;
  bool clone_newpid;
  bool clone_newns;
  bool clone_newtime;
  bool clone_keywords;
};

int rb_unshare_internal(struct rb_unshare_args args);

#endif