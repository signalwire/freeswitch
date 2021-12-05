#!/usr/bin/perl -w


#brief  Call single voicemail and print detection result to the console.
#author Piotr Gregor <piotr@dataandsignal.com>
#date   15 Sept 2016 02:44 PM


use strict;
use warnings;
require ESL;
use POSIX;
use Time::HiRes;

my $host = "127.0.0.1";
my $port = "8021";
my $pass = "ClueCon";
my $extension_base = "sofia/internal/1000\@192.168.1.60";

my $playback = "{loops=-1}tone_stream://%(251,0,1004)";
my $context = 'default'; 
my $endpoint;
my $dest;
my $callerid;


if ($#ARGV + 1 eq 2) {
	$dest = $ARGV[0];
	$callerid = $ARGV[1];
	print "Dialing [" .$dest ."] as " .$callerid ."]\n";
} else {
	die "Please specify destination number and caller id\n";
}

my $con  = new ESL::ESLconnection($host, $port, $pass);
if (!$con) {
	die "Unable to establish connection to $host:$port\n";
}
if ($con->connected()) {
	print "OK, Connected.\n";
} else {
	die "Connection failure.\n";
}

print "Subscribing to avmd events...\n";
$con->events("plain", "CUSTOM avmd::start");
$con->events("plain", "CUSTOM avmd::stop");
$con->events("plain", "CUSTOM avmd::beep");

while($con->connected()) {
	test_once($dest, $callerid);
	return 0;
}

print "Disconnected.\n\n";

sub test_once {
	my ($dest, $callerid) = @_;
	my $originate_string =
	'originate ' .
	'{ignore_early_media=true,' .
	'origination_uuid=%s,' . 
	'originate_timeout=60,' .
	'origination_caller_id_number=' . $callerid . ',' .
	'origination_caller_id_name=' . $callerid . '}';

	if(defined($endpoint)) {
		$originate_string .= $endpoint;
	} else {
		$originate_string .= 'loopback/' . $dest . '/' . $context;
	}
	$originate_string .=  ' ' . '&playback(' . $playback . ')';

	my $uuid = $con->api('create_uuid')->getBody();
	my ($time_epoch, $time_hires) = Time::HiRes::gettimeofday();
	printf("Calling with uuid [%s] [%s]... [%s]\n", $uuid, POSIX::strftime('%Y-%m-%d %H:%M:%S', localtime($time_epoch)), $originate_string);

	$con->bgapi(sprintf($originate_string, $uuid));

	print "Waiting for the events...\n\n";
	while($con->connected()) {
		my $e = $con->recvEvent();
		if ($e) {
			my $body = $e->serialize('plain');
			print $body;
			print "\n\n";
		}
	}
}
