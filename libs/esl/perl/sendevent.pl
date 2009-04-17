#!/usr/bin/perl

require ESL;

my $con = ESL::ESLconnection->new("localhost", "8021", "ClueCon");
my $e = ESL::ESLevent->new("MESSAGE_WAITING");

#my $e = $con->sendEvent("MESSAGE_WAITING");

$e->addHeader("MWI-Messages-Waiting", "no");
$e->addHeader("MWI-Message-Account", 'sip:1002@dev.bkw.org');
$e->addHeader("MWI-Voice-Message", "0/0 (0/0)");
$con->sendEvent($e);

sleep 3;
my $ee = ESL::ESLevent->new("MESSAGE_WAITING");

$ee->addHeader("MWI-Messages-Waiting", "yes");
$ee->addHeader("MWI-Message-Account", 'sip:1002@dev.bkw.org');
$ee->addHeader("MWI-Voice-Message", "1/1 (1/1)");
$con->sendEvent($ee);

