#!/usr/bin/perl
use IO::Socket::INET;

my ($ip, $port, $file) = @ARGV;

$ip and $port or die "Usage $0: <ip> <port>\n";

$socket = new IO::Socket::INET->new( PeerPort => $port,
				     Proto => 'udp',
				     PeerAddr => $ip);
open(I, $file);
$/ = undef;
my $buf = <I>;
close(I);

$socket->send("$buf\n");

