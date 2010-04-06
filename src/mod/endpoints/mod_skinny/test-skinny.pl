#!/usr/bin/perl

# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

BEGIN {
    push @INC, 'src/mod/endpoints/mod_skinny';
}

use strict;
use warnings;

use Sys::Hostname;
use Net::Skinny;
use Net::Skinny::Protocol qw/:all/;
use Net::Skinny::Message;

#Config
my $skinny_server = hostname;
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
$socket->send_message(
    REGISTER_MESSAGE,
    device_name => $device_name,
    user_id => 0,
    instance => 1,
    ip => $device_ip,
    device_type => 7,
    max_streams => 0,
    );
$socket->receive_message(); # RegisterAck

$socket->send_message(
    PORT_MESSAGE,
	port => 2000,
	);

$socket->send_message(
    HEADSET_STATUS_MESSAGE,
	mode => 2, #Off
	);
$socket->receive_message(); # CapabilitiesReq
$socket->send_message(
    CAPABILITIES_RES_MESSAGE,
    count => 2,
    caps => pack("Vva10"."Vva10",
		    2, 8, "", # codec, frames, res
		    4, 16, "", # codec, frames, res
	    )
	);

$socket->send_message(BUTTON_TEMPLATE_REQ_MESSAGE);
$socket->receive_message(); # ButtonTemplateMessage

$socket->send_message(SOFT_KEY_TEMPLATE_REQ_MESSAGE);
$socket->receive_message(); # SoftKeyTemplateRes

$socket->send_message(SOFT_KEY_SET_REQ_MESSAGE);
$socket->receive_message(); # SoftKeySetRes
$socket->receive_message(); # SelectSoftKeys

$socket->send_message(
    LINE_STAT_REQ_MESSAGE,
    number => 1,
    );
$socket->receive_message(); # LineStat

$socket->send_message(
    REGISTER_AVAILABLE_LINES_MESSAGE, 
    count => 2
    );

for(my $i = 0; $i < 1; $i++) {
	$socket->sleep(5);
	$socket->send_message(KEEP_ALIVE_MESSAGE);
	$socket->receive_message(); # keepaliveack
}
$socket->sleep(5);

#NewCall
$socket->send_message(
    SOFT_KEY_EVENT_MESSAGE, 
    event => 2, #NewCall
    line_instance => 2,
    call_id => 0
    );
$socket->receive_message(); # SetRinger
$socket->receive_message(); # SetSpeakerMode
$socket->receive_message(); # SetLamp
$socket->receive_message(); # SelectSoftKeys
$socket->receive_message(); # DisplayPromptStatus
$socket->receive_message(); # ActivateCallPlane
$socket->receive_message(); # StartTone

$socket->sleep(5);

#VoiceMail
$socket->send_message(
    STIMULUS_MESSAGE, 
    instance_type => 0xf, #VoiceMail
    instance => 0,
    );
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 
$socket->receive_message(); # 

#
$socket->send_message(
    OPEN_RECEIVE_CHANNEL_ACK_MESSAGE, 
    status => 1,
    ip => $device_ip,
    port => 12,
    pass_thru_party_id => 0,
    );
$socket->receive_message(); # StartMediaTransmission

$socket->sleep(20);

#EndCall
$socket->send_message(
    SOFT_KEY_EVENT_MESSAGE, 
    event => 0x09, #NewCall
    line_instance => 1,
    call_id => 0
    );

while(1) {
    $socket->receive_message();
}

