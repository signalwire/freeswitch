#!/usr/bin/perl

require ESL;

my $con = ESL::ESLconnection->new("localhost", "8021", "ClueCon");
my $e = ESL::ESLevent->new("SEND_INFO");

#my $e = $con->sendEvent("MESSAGE_WAITING");

$e->addHeader("Content-Type", "text/xml");
$e->addHeader("to-uri", 'sip:1002@dev.bkw.org');
$e->addHeader("from-uri", 'sip:1234@dev.bkw.org');
$e->addHeader("profile", 'external');

$con->sendEvent($e);
