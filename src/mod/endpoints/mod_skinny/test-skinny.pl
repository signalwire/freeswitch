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
use Net::Skinny::Client;

#Config
my $skinny_server = hostname;
my $device_name = "SEP001120AABBCC";
my $device_ip = 10+256*(11+256*(12+256*13)); # 10.11.12.13
#======
$| = 1;

my $socket = Net::Skinny::Client->new(
		PeerAddr => $skinny_server,
		PeerPort => 2000,
		);

if(!$socket) {
    printf "Unable to connect to server %s\n", $skinny_server;
    exit 1;
}
# =============================================================================
$socket->send_raw(
    XML_ALARM_MESSAGE,
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\x0a<x-cisco-alarm>\x0a<Alarm Name=\"LastOutOfServiceInformation\">\x0a<ParameterList>\x0a<String name=\"DeviceName\">SEP002699438F62</String>\x0a<String name=\"DeviceIPv4Address\">192.168.3.201/24</String>\x0a<String name=\"IPv4DefaultGateway\">192.168.3.254</String>\x0a<String name=\"DeviceIPv6Address\"></String>\x0a<String name=\"IPv6DefaultGateway\"></String>\x0a<String name=\"ModelNumber\">CP-7961G</String>\x0a<String name=\"NeighborIPv4Address\">192.168.0.253</String>\x0a<String name=\"NeighborIPv6Address\"></String>\x0a<String name=\"NeighborDeviceID\">sw2.wvds.local</String>\x0a<String name=\"NeighborPortID\">3</String>\x0a<Enum name=\"DHCPv4Status\">1</Enum>\x0a<Enum name=\"DHCPv6Status\">0</Enum>\x0a<Enum name=\"TFTPCfgStatus\">0</Enum>\x0a<Enum name=\"DNSStatusUnifiedCM1\">0</Enum>\x0a<Enum name=\"DNSStatusUnifiedCM2\">0</Enum>\x0a<Enum name=\"DNSStatusUnifiedCM3\">0</Enum>\x0a<String name=\"VoiceVLAN\">4095</String>\x0a<String name=\"UnifiedCMIPAddress\"><not open></String>\x0a<String name=\"LocalPort\">-1</String>\x0a<String name=\"TimeStamp\">1289313813826</String>\x0a<Enum name=\"ReasonForOutOfService\"></Enum>\x0a<String name=\"LastProtocolEventSent\">1:Register</String>\x0a<String name=\"LastProtocolEventReceived\">129:RegisterAck</String>\x0a</ParameterList>\x0a</Alarm>\x0a</x-cisco-alarm>\x0a"
    );
    $socket->sleep(20);
    exit;
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

if(0) {
	$socket->send_message(VERSION_REQ_MESSAGE);
	$socket->receive_message(); # VersionMessage
}

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

$socket->launch_keep_alive_thread();

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

