#! /usr/bin/ruby
require 'ESL' 

HOST     = '127.0.0.1'.to_s 
PORT     = '8021'.to_s 
PASSWORD = 'ClueCon'.to_s 

command = ARGV.join(" ")

con = ESL::ESLconnection.new(HOST,PORT,PASSWORD) 
e = con.sendRecv('api ' + command)

puts e.getBody

