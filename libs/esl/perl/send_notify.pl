#!/usr/bin/perl

require ESL;

ESL::eslSetLogLevel(7);

my $con = ESL::ESLconnection->new("localhost", "8021", "ClueCon");
my $e = ESL::ESLevent->new("NOTIFY");


$e->addHeader("from-uri", "sip:1000\@dev.bkw.org");
$e->addHeader("to-uri", "sip:1000\@dev.bkw.org");
$e->addHeader("event-string", "message-summary");
$e->addHeader("content-type", "application/simple-message-summary");
$e->addHeader("profile", "internal");


my $body ="Messages-Waiting: yes\nMessage-Account: me\@my.com\nVoice-Message: 0/0 (0/0)\n";
$e->addBody($body);
$con->sendEvent($e);

