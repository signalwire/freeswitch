#!/usr/local/bin/perl
use IO::Socket::INET;

my ($ip, $port, $file) = @ARGV;

$ip and $port or die "Usage $0: <ip> <port>\n";

$socket = new IO::Socket::INET->new( PeerPort => $port,
				     Proto => 'udp',
				     PeerAddr => $ip);

my $buf = `cat $file`;

$socket->send("$buf\n");

