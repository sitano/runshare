require "mkmf"

append_cflags('-D_GNU_SOURCE')
append_cflags('-DPACKAGE_STRING="\"runshare\""')
append_cflags('-Wno-discarded-qualifiers')

if have_func("nanosleep", "time.h")
    $defs.push("-DHAVE_NANOSLEEP")
elsif have_func("usleep", "time.h")
    $defs.push("-DHAVE_USLEEP")
end

if have_func("fsync", "unistd.h")
    $defs.push("-DHAVE_FSYNC")
end

if have_func("vasprintf", "stdio.h")
    $defs.push("-DHAVE_VASPRINTF")
end

if have_func("unshare", "sys/syscall.h")
    $defs.push("-DHAVE_UNSHARE")
end
if have_func("setns", "sys/syscall.h")
    $defs.push("-DHAVE_SETNS")
end
if have_func("err", "err.h")
    $defs.push("-DHAVE_ERR_H")
end

$srcs = ["runshare.c", "unshare.c"]

create_makefile("runshare/runshare")
