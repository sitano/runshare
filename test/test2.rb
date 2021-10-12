# rake compile && sudo ruby -I ./lib ./test/test2.rb

require "runshare"

puts Process.pid
(1..2).each {
  fork {
    parent = Process.pid
    puts "- #{parent}"

    res = RUnshare::unshare(:clone_newpid => true, :fork => true)
    puts "-- unshare=#{res}, pid=#{Process.pid}"

    is_child = parent != Process.pid
    if is_child then
      fork {
        puts "--- #{Process.pid}"
        sleep 10
      }
    end
  }
}

puts 'done'

sleep 10

