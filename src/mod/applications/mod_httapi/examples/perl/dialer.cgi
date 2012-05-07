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
$writer->emptyTag('pause', milliseconds => "1500");
$writer->startTag('playback', 
		  name => digits, 
		  file => "http://sidious.freeswitch.org/sounds/exten.wav",
		  'error-file' => "http://sidious.freeswitch.org/sounds/invalid.wav",
		  'input-timeout' => "5000",
		  action => "dial:default:XML");

$writer->dataElement("bind", "~\\d+\#", strip => "#");
$writer->endTag('playback');
$writer->endTag('work');


$writer->endTag('document');
$writer->end();

