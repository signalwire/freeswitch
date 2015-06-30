#!/usr/bin/perl
#
# Used for testing, This will allow you to configure a
# dialplan on a remote system for testing via X headers
# in a SIP invite.  DO NOT RUN IN PRODUCTION LIKE THIS.
#
# YOU'VE BEEN WARNED!
#

use CGI;
use XML::Writer;
use IO::File;
use Data::Dumper;

my $q = CGI->new;
my $output = IO::File->new(*STDOUT);
my $writer = XML::Writer->new(OUTPUT => $output, DATA_MODE => 1, DATA_INDENT => 2);

my $params = $q->Vars;
print $q->header('text/xml');
$writer->xmlDecl("UTF-8");

$writer->startTag("document", "type" => "freeswitch/xml");

if($params->{'Hunt-Destination-Number'} eq 'puppet') {
    $writer->startTag("section",  "name" => "dialplan");
    $writer->startTag("context",  "name" => "$params->{'Hunt-Context'}");
    $writer->startTag("extension", "name" => "puppet");
    $writer->startTag("condition");
    my $count = 1;
    while (exists $params->{"variable_sip_h_X-DP-$count"}) {
	my ($app, $arg) = split(/:/, $params->{"variable_sip_h_X-DP-$count"});
	if($arg) {
	    $writer->emptyTag("action", "application"  => "$app", "data" => "$arg");
	} else {
	    $writer->emptyTag("action", "application"  => "$app");
	}
	$count++;
    }
    $writer->endTag("condition");
    $writer->endTag("extension");
    $writer->endTag("context");
} else {
    $writer->startTag("section",  "name" => "result");
    $writer->emptyTag("result", "status"  => "not found");
}
$writer->endTag("section");
$writer->endTag("document");
$writer->end();
