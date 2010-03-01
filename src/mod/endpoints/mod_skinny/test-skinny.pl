#!/usr/bin/perl

use strict;
use warnings;
use IO::Socket;

#Config
my $skinny_server = '127.0.0.1';
my $device_name = "SEP001120AABBCC";
my $device_ip = 10+256*(11+256*(12+256*13)); # 10.11.12.13
#======
$| = 1;

my $socket;

sub skinny_connect
{
	$socket = IO::Socket::INET->new(
		PeerAddr => $skinny_server,
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

# =============================================================================
# 
# =============================================================================
skinny_connect();

# =============================================================================
skinny_send(0x0001, # Register
	pack("a16VVVVV",
		$device_name,
		0, # userId;
		1, # instance;
		$device_ip,# ip;
		7, # deviceType;
		0, # maxStreams;
	));
skinny_recv(); # RegisterAck

skinny_send(0x0002, # Port
	pack("n", 2000
	));

skinny_send(0x002b, # HeadSetStatus
	pack("V",
		2, # Off
	));

skinny_recv(); # CapabilitiesReq
skinny_send(0x0010, # CapabilitiesRes
	pack("V"."Vva10"."Vva10",
		2, # count
		2, 8, "", # codec, frames, res
		4, 16, "", # codec, frames, res
	));

skinny_send(0x000e, # ButtonTemplateReqMessage
	"");
skinny_recv(); # ButtonTemplateMessage

skinny_send(0x0028, # SoftKeyTemplateReq
	"");
skinny_recv(); # SoftKeyTemplateRes

skinny_send(0x0025, # SoftKeySetReq
	"");
skinny_recv(); # SoftKeySetRes

skinny_send(0x000B, # LineStatReq
	pack("V", 1));
skinny_recv(); # LineStat

skinny_send(0x002D, # RegisterAvailableLines
	pack("V", 2
	));

while(1) {
	skinny_sleep(20);
	skinny_send(0x0000, # keepalive
		"");
	skinny_recv(); # keepaliveack
}
