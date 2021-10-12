require "runshare"

puts Process.pid
(1..2).each {
  fork {
    puts "- #{Process.pid}"
    RUnshare::unshare(:clone_newpid => true)
    puts "-- #{Process.pid}"
    fork {
      puts "--- #{Process.pid}"
      sleep 10
    }
  }
}


puts 'done'

sleep 10

