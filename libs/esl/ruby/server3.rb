#!/usr/bin/ruby

# Simple Ruby ESL example that forks a new process for every connection.
# Called like this: <action application="socket" data="localhost:8086 async full"/>
#
# Try pressing 5, 8 or 9 when the file is played and see what happens
# 
# Contributed by Mikael Bjerkeland

require "ESL"
require 'socket'
include Socket::Constants
bind_address = "127.0.0.1"
bind_port = 8086

def time_now
  Time.now.strftime("%Y-%m-%d %H:%M:%S")
end

socket = Socket.new(AF_INET, SOCK_STREAM, 0)
sockaddr = Socket.sockaddr_in(bind_port, bind_address)
socket.bind(sockaddr)
socket.listen(5)
puts "Listening for connections on #{bind_address}:#{bind_port}"
loop do 
  client_socket, client_sockaddr = socket.accept #_nonblock
  pid = fork do
    @con = ESL::ESLconnection.new(client_socket.fileno)
    info = @con.getInfo
    uuid = info.getHeader("UNIQUE-ID")
    @con.sendRecv("myevents")
    @con.sendRecv("divert_events on")

    puts "#{time_now} [#{uuid}] Call to [#{info.getHeader("Caller-Destination-Number")}]"
    @con.execute("log", "1, Wee-wa-wee-wa")
    @con.execute("info", "")
    @con.execute("answer", "")
    @con.execute("playback", "/usr/local/freeswitch/sounds/music/8000/suite-espanola-op-47-leyenda.wav")

    while @con.connected
      e = @con.recvEvent

      if e
        name = e.getHeader("Event-Name")
        puts "EVENT: #{name}"
        break if name == "SERVER_DISCONNECTED"
        if name == "DTMF"
          digit = e.getHeader("DTMF-DIGIT")
          duration = e.getHeader("DTMF-DURATION")
          puts "DTMF DIGIT #{digit} (#{duration})"
          if digit == "5"
            @con.execute("log", "1, WHA HA HA. User pressed 5")
          elsif digit == "8"
            # TODO: close connection without hangup in order to proceed in dialplan. How?
          elsif digit == "9"
            @con.execute("transfer", "99355151")
          end
        end

      end 
    end
    puts "Connection closed"
  end

  Process.detach(pid)
end
