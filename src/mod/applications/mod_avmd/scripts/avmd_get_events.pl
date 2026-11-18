#!/usr/bin/perl -w


#brief  Subscribe to avmd events and print them to the console.
#author Piotr Gregor <piotr@dataandsignal.com>
#date   13 Sept 2016 09:44 PM


$|++;   # turn on autoflush
use strict;
use warnings;
require ESL;


my $host = "127.0.0.1";
my $port = "8021";
my $pass = "ClueCon";

my $format = "plain";

if ($#ARGV + 1 eq 1) {
	$format = $ARGV[0];
	print "Using format: [" .$format ."]\n";
}

my $con = new ESL::ESLconnection($host, $port, $pass);
if (!$con) {
	die "Unable to establish connection to $host:$port\n";
}
if ($con->connected()) {
	print "OK, Connected.\n";
} else {
	die "Conenction failure.\n";
}

print "Subscribing to avmd events...\n";
$con->events("plain", "CUSTOM avmd::start");
$con->events("plain", "CUSTOM avmd::stop");
$con->events("plain", "CUSTOM avmd::beep");

print "Waiting for the events...\n";
while($con->connected()) {
	my $e = $con->recvEvent();
	my $avmd_event_type = "";
	$avmd_event_type = $e->getHeader("Event-Subclass");
	if ($avmd_event_type eq 'avmd::start') { # mark nicely the start of new session and event streak - most likely there will be other events from this session coming after this one
		print "\n--------------------\n\n";
	}
	if ($e) {
		my $body = $e->serialize($format);
		print $body;
		print "\n\n";
	}
}

print "Disconnected.\n\n";
