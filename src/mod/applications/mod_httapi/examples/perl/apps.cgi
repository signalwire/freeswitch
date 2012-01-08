#!/usr/bin/perl

# Object initialization:
use XML::Simple;
use CGI;
use Data::Dumper;
use XML::Writer;

my $q = CGI->new;
my $exiting = $q->param("exiting");

if ($exiting) {
    print $q->header(-type => "text/plain");
    print "OK";
    exit();
}

print $q->header(-type => "text/xml");


my $writer = new XML::Writer(OUTPUT => STDOUT, DATA_MODE => 1);

$writer->startTag('document', type => 'xml/freeswitch-httapi');

$writer->startTag('work');
$writer->emptyTag('pause', milliseconds => "500");
$writer->emptyTag('execute', application => "info");
$writer->dataElement('execute', "user_busy", application => "hangup");
$writer->endTag('work');


$writer->endTag('document');
$writer->end();

