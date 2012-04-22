#!/usr/bin/perl

require ESL;

ESL::eslSetLogLevel(7);

my $con = ESL::ESLconnection->new("localhost", "8021", "ClueCon");
my $e = ESL::ESLevent->new("SEND_INFO");


$e->addHeader("local-user", '1000@192.168.1.113');
$e->addHeader("from-uri", 'sip:1000@192.168.1.113');
$e->addHeader("to-uri", 'sip:1000\@192.168.1.113');
$e->addHeader("content-type", "application/csta+xml");
$e->addHeader("content-disposition", "signal; handling=required");

$e->addHeader("profile", "internal");


my $body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<MakeCall xmlns=\"http://www.ecma-international.org/standards/ecma-323/csta/ed3\">
 <callingDevice>sip:1000@\192.168.1.113</callingDevice>
 <calledDirectoryNumber>sip:9999\@192.168.1.113</calledDirectoryNumber>
 <autoOriginate>doNotPrompt</autoOriginate>
</MakeCall>";

$e->addBody($body);
$con->sendEvent($e);

