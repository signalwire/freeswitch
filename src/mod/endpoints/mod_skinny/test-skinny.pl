#!/usr/bin/perl

# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

BEGIN {
    push @INC, 'src/mod/endpoints/mod_skinny';
}

use strict;
use warnings;

use Data::Dumper;
use Net::Skinny;
use Net::Skinny::Protocol qw/:all/;

#Config
my $skinny_server = '127.0.0.1';
my $device_name = "SEP001120AABBCC";
my $device_ip = 10+256*(11+256*(12+256*13)); # 10.11.12.13
#======
$| = 1;

my $socket = Net::Skinny->new(
		PeerAddr => $skinny_server,
		PeerPort => 2000,
		);

if(!$socket) {
    print "Unable to connect to server\n";
    exit 1;
}
# =============================================================================
$socket->send_message(REGISTER_MESSAGE, # Register
	pack("a16VVVVV",
		$device_name,
		0, # userId;
		1, # instance;
		$device_ip,# ip;
		7, # deviceType;
		0, # maxStreams;
	));
$socket->receive_message(); # RegisterAck

$socket->send_message(0x0002, # Port
	pack("n", 2000
	));

$socket->send_message(HEADSET_STATUS_MESSAGE,
	pack("V",
		2, # Off
	));

$socket->receive_message(); # CapabilitiesReq
$socket->send_message(CAPABILITIES_RES_MESSAGE,
	pack("V"."Vva10"."Vva10",
		2, # count
		2, 8, "", # codec, frames, res
		4, 16, "", # codec, frames, res
	));

$socket->send_message(BUTTON_TEMPLATE_REQ_MESSAGE, "");
$socket->receive_message(); # ButtonTemplateMessage

$socket->send_message(SOFT_KEY_TEMPLATE_REQ_MESSAGE, "");
$socket->receive_message(); # SoftKeyTemplateRes

$socket->send_message(SOFT_KEY_SET_REQ_MESSAGE,	"");
$socket->receive_message(); # SoftKeySetRes

$socket->send_message(LINE_STAT_REQ_MESSAGE, pack("V", 1));
$socket->receive_message(); # LineStat

$socket->send_message(REGISTER_AVAILABLE_LINES_MESSAGE, pack("V", 2));

while(1) {
	$socket->sleep(20);
	$socket->send_message(KEEP_ALIVE_MESSAGE, "");
	$socket->receive_message(); # keepaliveack
}
