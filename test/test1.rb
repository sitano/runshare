require "runshare"

puts Process.pid
RUnshare::unshare(:clone_newpid => true)
RUnshare::unshare(:clone_newpid => true)
sleep 60
