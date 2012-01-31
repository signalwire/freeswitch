#!/usr/bin/perl

# Object initialization:
use XML::Simple;
use CGI;
use Data::Dumper;
use XML::Writer;

my $q = CGI->new;
my $exiting = $q->param("exiting");

my $file = $q->upload("recorded_file");

if ($file) {
  open(O, ">/tmp/recording.wav");
  while(<$file>) {
    print O $_;
  }
  close O;

  print $q->header(-type => "text/plain");
  print "OK\n";
  exit();
}

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
$writer->emptyTag('playback', file => "http://sidious.freeswitch.org/eg/ivr-say_name.wav");
$writer->startTag('record', 
		  name => "recorded_file", 
		  file => "recording.wav",
		  'error-file' => "http://sidious.freeswitch.org/sounds/invalid.wav",
		  'input-timeout' => "5000",
		  'beep-file', => "tone_stream://%(1000,0,460)");
		   

$writer->dataElement("bind", "~\\d+\#", strip => "#");
$writer->endTag('record');

$writer->endTag('work');


$writer->endTag('document');
$writer->end();

