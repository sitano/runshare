# rake compile && sudo ruby -I ./lib ./test/test3.rb
#
# sudo lsns | grep test5
# sudo ip link add veth0 type veth peer name veth1
# sudo ip addr add 10.1.1.1/24 dev veth0
# sudo ip addr add 10.1.1.2/24 dev veth1
# sudo ip link set dev veth0 up
# sudo ip link set dev veth1 up
# sudo ip link set veth1 netns 4026533350

require "runshare"

puts Process.pid
(1..2).each {
  fork {
    parent = Process.pid
    puts "- #{parent}"

    pid = RUnshare::unshare(
      :clone_newpid    => true,
      :clone_newns     => true,
      :clone_newcgroup => true,
      :clone_newipc    => true,
      :clone_newuts    => true,
      :clone_newnet    => true,
      :clone_newtime   => true,
      :fork            => true,
      :mount_proc      => "/proc",
      # docker export $(docker create praqma/network-multitool) | tar -xf - -C rootfs
      :root            => "/tmp/rootfs"
    )

    if pid == 0
      # child
      puts "--- #{Process.pid}"
      while system("/bin/ping 10.1.1.1") != true
        puts "bad"
        sleep 5
      end
      puts "--- wut"
    else
      # parent
      puts "-- unshare=#{pid}, pid=#{Process.pid}"
    end

    # parent+child exit
  }
}

sleep 3600

puts 'done'
