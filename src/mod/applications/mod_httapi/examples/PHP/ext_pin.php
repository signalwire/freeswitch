<?php
if ( array_key_exists( 'session_id', $_REQUEST ) ) {
  session_id( $_REQUEST['session_id'] );
}
session_start();

$xml = new XMLWriter();
$xml->openMemory();
$xml->setIndent(1);
$xml->startDocument();

if ( array_key_exists( 'exten',   $_REQUEST ) ) {
  $exten   = $_REQUEST['exten'];
} elseif ( array_key_exists( 'exten',   $_SESSION ) ) {
  $exten   = $_SESSION['exten'];
} else {
  $exten   = '';
}

if ( array_key_exists( 'pin',   $_REQUEST ) ) {
  $pin   = $_REQUEST['pin'];
} elseif ( array_key_exists( 'pin',   $_SESSION ) ) {
  $pin   = $_SESSION['pin'];
} else {
  $pin   = '';
}

if ( array_key_exists( 'exiting',   $_REQUEST ) ) {
  $exiting   = $_REQUEST['exiting'];
} elseif ( array_key_exists( 'exiting',   $_SESSION ) ) {
  $exiting   = $_SESSION['exiting'];
} else {
  $exiting   = '';
}

if ( $exiting ) {
  header('Content-Type: text/plain');
  print "OK";
  exit();
}

header('Content-Type: text/xml');
$xml->startElement('document');
$xml->writeAttribute('type', 'xml/freeswitch-httapi');

if ( $exten && $pin ) {
  $xml->startElement('work');
  $xml->writeElement("playback", "http://sidious.freeswitch.org/sounds/ext_num.wav");

  $xml->startElement("say");
  $xml->writeAttribute('language', "en");
  $xml->writeAttribute('type', "name_spelled");
  $xml->writeAttribute('method', "pronounced");
  $xml->text($exten);
  $xml->endElement(); // </say>

  $xml->startElement('pause');
  $xml->writeAttribute('milliseconds', "1500");
  $xml->endElement(); // </pause>
  
  $xml->startElement("say");
  $xml->writeAttribute('language', "en");
  $xml->writeAttribute('type', "name_spelled");
  $xml->writeAttribute('method', "pronounced");
  $xml->text($pin);
  $xml->endElement(); // </say>

  $xml->writeElement('hangup');
  $xml->endElement(); // </work>
} elseif ( $exten ) {
  $_SESSION['exten'] = $exten;

  $xml->startElement('work');
  $xml->startElement('playback');
  $xml->writeAttribute('name', "pin");
  $xml->writeAttribute('file', "http://sidious.freeswitch.org/sounds/pin.wav");
  $xml->writeAttribute('error-file', "http://sidious.freeswitch.org/sounds/bad-pin.wav");
  $xml->writeAttribute('input-timeout', "5000");

  $xml->startElement("bind");
  $xml->writeAttribute('strip', "#");
  $xml->text("~\\d+\#");
  $xml->endElement(); // </bind>

  $xml->endElement(); // </playback>
  $xml->endElement(); // </work>
} else {
  $xml->startElement('work');

  $xml->startElement('playback');
  $xml->writeAttribute('name', "exten");
  $xml->writeAttribute('file', "http://sidious.freeswitch.org/sounds/exten.wav");
  $xml->writeAttribute('loops', "3");
  $xml->writeAttribute('error-file', "http://sidious.freeswitch.org/sounds/invalid.wav");
  $xml->writeAttribute('input-timeout', "5000");

  $xml->startElement("bind");
  $xml->writeAttribute('strip', "#");
  $xml->text("~\\d+\#");
  $xml->endElement(); // </bind>

  $xml->endElement(); // </playback>
  $xml->endElement(); // </work>
}

$xml->endElement(); // </document>

print $xml->outputMemory();



