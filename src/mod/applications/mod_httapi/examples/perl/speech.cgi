#!/usr/bin/perl

# Object initialization:
use XML::Simple;
use CGI;
use Data::Dumper;
use XML::Writer;

my $q = CGI->new;

my $result = $q->param("result");
my $type = $q->param("input_type");
my $exiting = $q->param("exiting");

if ($exiting) {
    print $q->header(-type => "text/plain");
    print "OK";
    exit();
}

print $q->header(-type => "text/xml");


my $writer = new XML::Writer(OUTPUT => STDOUT, DATA_MODE => 1);

$writer->startTag('document', type => 'xml/freeswitch-httapi');

if ($result) {
    $writer->startTag('work');

    if ($type eq "dtmf") {
      $writer->dataElement("say", $result, language => "en", type => "name_spelled", method => "pronounced");
    }

    $writer->dataElement("log", $result, level => "crit");

    $writer->emptyTag('hangup');
    $writer->endTag('work');
} else {

    $writer->startTag('work');
    $writer->emptyTag('pause', milliseconds => "1500");
    $writer->startTag('playback',
		      name => "result",
		      'asr-engine' => "pocketsphinx", 
		      'asr-grammar' => "pizza_yesno",
		      file => "http://sidious.freeswitch.org/sounds/ConfirmDelivery.wav",
		      'error-file' => "http://sidious.freeswitch.org/sounds/invalid.wav"
	);

    $writer->dataElement("bind", "~\\d+\#", strip => "#");
    #$writer->dataElement("bind", "1");
    #$writer->dataElement("bind", "2");
    $writer->endTag('playback');

    $writer->endTag('work');
}

$writer->endTag('document');
$writer->end();

