#include "runshare.h"

VALUE rb_mRUnshare;

VALUE sym_clone_newuser;
VALUE sym_clone_newcgroup;
VALUE sym_clone_newipc;
VALUE sym_clone_newuts;
VALUE sym_clone_newnet;
VALUE sym_clone_newpid;
VALUE sym_clone_newns;
VALUE sym_clone_newtime;

static VALUE
rb_unshare(VALUE mod)
{
  int err = rb_unshare_internal(1, (char **) (const char *[1]) {"runshare"});
  return INT2FIX(err);
}

void
Init_runshare(void)
{
  rb_mRUnshare = rb_define_module("RUnshare");
  rb_define_singleton_method(rb_mRUnshare, "unshare", rb_unshare, 0);

  sym_clone_newuser = ID2SYM(rb_intern("CLONE_NEWUSER"));
  sym_clone_newcgroup = ID2SYM(rb_intern("CLONE_NEWCGROUP"));
  sym_clone_newipc = ID2SYM(rb_intern("CLONE_NEWIPC"));
  sym_clone_newuts = ID2SYM(rb_intern("CLONE_NEWUTS"));
  sym_clone_newnet = ID2SYM(rb_intern("CLONE_NEWNET"));
  sym_clone_newpid = ID2SYM(rb_intern("CLONE_NEWPID"));
  sym_clone_newns = ID2SYM(rb_intern("CLONE_NEWNS"));
  sym_clone_newtime = ID2SYM(rb_intern("CLONE_NEWTIME"));
}
