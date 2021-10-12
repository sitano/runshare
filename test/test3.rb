# rake compile && sudo ruby -I ./lib ./test/test3.rb

require "runshare"

puts Process.pid
(1..2).each {
  fork {
    parent = Process.pid
    puts "- #{parent}"

    pid = RUnshare::unshare(
      :clone_newpid => true,
      :fork => true,
      :mount_proc => "/proc",
      :root => "/")
    puts "-- unshare=#{pid}, pid=#{Process.pid}"

    if pid == 0 then
      # child
      puts "--- #{Process.pid}"
      sleep 10
    end

    # parent+child exit
  }
}

sleep 10

puts 'done'
