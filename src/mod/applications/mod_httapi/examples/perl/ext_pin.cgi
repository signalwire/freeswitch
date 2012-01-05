#!/usr/bin/perl

# Object initialization:
use XML::Simple;
use CGI;
use Data::Dumper;
use XML::Writer;

my $q = CGI->new;

my $exten = $q->param("exten");
my $pin = $q->param("pin");
my $exiting = $q->param("exiting");

if ($exiting) {
    print $q->header(-type => "text/plain");
    print "OK";
    exit();
  }

print $q->header(-type => "text/xml");

my $writer = new XML::Writer(OUTPUT => STDOUT, DATA_MODE => 1);

$writer->startTag('document', type => 'xml/freeswitch-httapi');

$writer->startTag('params');
if ($exten) {
  $writer->dataElement("exten", $exten);
}
if ($pin) {
  $writer->dataElement("exten", $pin);
}
$writer->endTag('params');

if ($exten eq "invalid" || $pin eq "invalid") {
    $writer->startTag('work');
    $writer->emptyTag('hangup', cause => "destination_out_of_order");
    $writer->endTag('work');
}

if ($exten && $pin) {
  $writer->startTag('work');
  $writer->dataElement("playback", "http://sidious.freeswitch.org/sounds/ext_num.wav");
  $writer->dataElement("say", $exten, language => "en", type => "name_spelled", method => "pronounced");
  $writer->emptyTag('pause', milliseconds => "1500");
  $writer->dataElement("say", $pin, language => "en", type => "name_spelled", method => "pronounced");
  $writer->emptyTag('hangup');
  $writer->endTag('work');
} elsif ($exten) {
  $writer->startTag('work');
  $writer->startTag('playback', 
		    name => "pin", 
		    file => "http://sidious.freeswitch.org/sounds/pin.wav",
		    'error-file' => "http://sidious.freeswitch.org/sounds/bad-pin.wav",
		    'input-timeout' => "5000");
		   

  $writer->dataElement("bind", "~\\d+\#", strip => "#");
  $writer->endTag('playback');
  $writer->endTag('work');
} else {
  $writer->startTag('work');
  $writer->startTag('playback', 
		    name => "exten", 
		    file => "http://sidious.freeswitch.org/sounds/exten.wav",
		    loops => "3",
		    'error-file' => "http://sidious.freeswitch.org/sounds/invalid.wav",
		    'input-timeout' => "5000");

  $writer->dataElement("bind", "~\\d+\#", strip => "#");
  $writer->endTag('playback');
  $writer->endTag('work');
}

$writer->endTag('document');
$writer->end();

