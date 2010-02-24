#!/usr/bin/perl

use strict;
use warnings;
use IO::Socket;

$| = 1;

my $socket;

sub skinny_connect
{
	$socket = IO::Socket::INET->new(
		PeerAddr => '192.168.0.6',
		PeerPort => 2000,
		);
}

sub skinny_send
{
	my $type = shift;
	my $data = shift;
	my $len = length($data)+4;
	printf "Sending message (length=%d, type=%X)", $len, $type;
	$socket->send(
		pack("VVV", $len, 0, $type).
		$data);
	printf ".\n";
}

sub skinny_recv
{
	my $buf;
	$socket->recv($buf, 4);
	my $len = unpack("V", $buf);
	printf "Receiving message (length=%d,", $len;
	if($len < 4) {
		printf "type=?).\n";
		printf "Problem! Length is < 4.\n";
		exit 1;
	}
	$socket->recv($buf, 4); #reserved
	$socket->recv($buf, 4); #type
	my $type = unpack("V", $buf);
	printf "type=%X)", $type;
	if($len > 4) {
		$socket->recv($buf, $len-4);
	}
	printf ".\n";
}

sub skinny_sleep
{
	my $t = shift;
	
	printf "Sleeping %d seconds", $t;
	while(--$t){
		sleep(1);
		printf "." if $t % 10;
		printf "_" unless $t % 10;
	}
	printf ".\n";
}

skinny_connect();

#
skinny_send(0x0001, # register
	pack("a16VVVVV",
		"SEP001120AABBCC",
		0, # userId;
		1, # instance;
		12,# ip;
		7, # deviceType;
		0, # maxStreams;
	));
skinny_recv(); # registerack

skinny_send(0x0002, # port
	pack("n", 2000
	));

skinny_recv(); # capreq
skinny_send(0x0010, # capres
	pack("V"."Vva10"."Vva10",
		2, # count
		2, 8, "", # codec, frames, res
		4, 16, "", # codec, frames, res
	));

skinny_send(0x000B, # linestatreq
	pack("V", 1));
skinny_recv(); # linestatres

skinny_send(0x002D, # registeravlines
	pack("V", 2
	));

while(1) {
	skinny_sleep(20);
	skinny_send(0x0000, # keepalive
		"");
	skinny_recv(); # keepaliveack
}
