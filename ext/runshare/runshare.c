#include <ruby.h>

#include "unshare.h"

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

static ID rb_unshare_keywords[FLAGS_COUNT];

static VALUE
rb_unshare(int argc, VALUE *argv, VALUE self)
{
  VALUE opt = Qnil;
  struct rb_unshare_args args = {};

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
    if (kwvals[CLONE_NEWNS]  != Qundef) args.clone_newns = RTEST(kwvals[CLONE_NEWNS]);
    if (kwvals[CLONE_NEWTIME] != Qundef) args.clone_newtime = RTEST(kwvals[CLONE_NEWTIME]);
    if (kwvals[FORK_ON_CLONE] != Qundef) args.fork = RTEST(kwvals[FORK_ON_CLONE]);
  }

  return INT2FIX(rb_unshare_internal(args));
}

void
Init_runshare(void) {
  rb_mRUnshare = rb_define_module("RUnshare");
  rb_define_singleton_method(rb_mRUnshare, "unshare", rb_unshare, -1);

  id_clone_newuser   = rb_intern("clone_newuser");
  id_clone_newcgroup = rb_intern("clone_newcgroup");
  id_clone_newipc    = rb_intern("clone_newipc");
  id_clone_newuts    = rb_intern("clone_newuts");
  id_clone_newnet    = rb_intern("clone_newnet");
  id_clone_newpid    = rb_intern("clone_newpid");
  id_clone_newns     = rb_intern("clone_newns");
  id_clone_newtime   = rb_intern("clone_newtime");
  id_fork            = rb_intern("fork");

  rb_unshare_keywords[CLONE_NEWUSER] = id_clone_newuser;
  rb_unshare_keywords[CLONE_NEWCGROUP] = id_clone_newcgroup;
  rb_unshare_keywords[CLONE_NEWIPC] = id_clone_newipc;
  rb_unshare_keywords[CLONE_NEWUTS] = id_clone_newuts;
  rb_unshare_keywords[CLONE_NEWNET] = id_clone_newnet;
  rb_unshare_keywords[CLONE_NEWPID] = id_clone_newpid;
  rb_unshare_keywords[CLONE_NEWNS] = id_clone_newns;
  rb_unshare_keywords[CLONE_NEWTIME] = id_clone_newtime;
  rb_unshare_keywords[FORK_ON_CLONE] = id_fork;
}