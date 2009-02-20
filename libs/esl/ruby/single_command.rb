#! /usr/bin/ruby

require "ESL"

command = ARGV.join(" ")
con = ESL::ESLconnection.new("localhost", "8021", "ClueCon")
e = con.sendRecv("api #{command}")
puts e.getBody()
