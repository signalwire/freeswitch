#! /usr/bin/ruby

#require "ESL"
require "../../../../libs/esl/ruby/ESL"
tries=10000

con = ESL::ESLconnection.new("localhost", "8021", "ClueCon")
e = con.sendRecv("api load mod_memcache")
puts e.getBody()
e = con.sendRecv("api reload mod_memcache")
puts e.getBody()
puts "Calling various memcache apis #{tries} times"
tries.times do |try|
  if (try % 100 == 0) then
    puts try
  end
  e = con.sendRecv("api memcache add foo a#{try}")
  e = con.sendRecv("api memcache set foo s#{try}")
  e = con.sendRecv("api memcache replace foo r#{try}")
  e = con.sendRecv("api memcache get foo #{try}")
  e = con.sendRecv("api memcache increment foo")
  e = con.sendRecv("api memcache decrement foo")
  e = con.sendRecv("api memcache delete foo")
end

e = con.sendRecv("api memcache flush")
e = con.sendRecv("api memcache status verbose")
puts e.getBody()
