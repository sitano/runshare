# rake compile && sudo ruby -I ./lib ./test/test3.rb

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
      # docker export $(docker create hello-world) | tar -xf - -C rootfs
      :root            => "/tmp/rootfs"
    )

    if pid == 0
      # child
      puts "--- #{Process.pid}"
      if system("/hello") != true
        raise "bad"
      end
      puts "--- wut"
    else
      # parent
      puts "-- unshare=#{pid}, pid=#{Process.pid}"
    end

    # parent+child exit
  }
}

sleep 10

puts 'done'
