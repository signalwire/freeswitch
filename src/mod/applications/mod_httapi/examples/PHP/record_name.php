<?php
if ( array_key_exists( 'session_id', $_REQUEST ) ) {
  session_id( $_REQUEST['session_id'] );
}
session_start();

$xml = new XMLWriter();
$xml->openMemory();
$xml->setIndent(1);
$xml->startDocument();

if ( array_key_exists( 'exiting',   $_REQUEST ) ) {
  $exiting   = $_REQUEST['exiting'];
} elseif ( array_key_exists( 'exiting',   $_SESSION ) ) {
  $exiting   = $_SESSION['exiting'];
} else {
  $exiting   = '';
}

if ( $_FILES && array_key_exists( 'recorded_file', $_FILES ) ) {
  move_uploaded_file($_FILES['recorded_file']['tmp_name'], '/tmp/' . $_FILES['recorded_file']['name']);
  trigger_error( print_r( $_FILES, true ) );

  header('Content-Type: text/plain');
  print "OK\n";
  exit();
}

if ( $exiting ) {
  header('Content-Type: text/plain');
  print "OK";
  exit();
}

header('Content-Type: text/xml');
$xml->startElement('document');
$xml->writeAttribute('type', 'xml/freeswitch-httapi');

$xml->startElement('work');

$xml->startElement('pause');
$xml->writeAttribute('milliseconds', "1500");
$xml->endElement();

$xml->startElement('playback');
$xml->writeAttribute('file', "http://sidious.freeswitch.org/eg/ivr-say_name.wav");
$xml->endElement();

$xml->startElement('record');
$xml->writeAttribute('name', "recorded_file");
$xml->writeAttribute('file', $_REQUEST['session_id'] . ".wav");
$xml->writeAttribute('error-file', "http://sidious.freeswitch.org/sounds/invalid.wav");
$xml->writeAttribute('input-timeout', "5000");
$xml->writeAttribute('beep-file', "tone_stream://%(1000,0,460)");
$xml->endElement();

$xml->startElement("bind");
$xml->writeAttribute('strip', "#");
$xml->text("~\\d+\#");
$xml->endElement();



$xml->endElement(); // </work>

$xml->endElement(); // </document>

print $xml->outputMemory();



