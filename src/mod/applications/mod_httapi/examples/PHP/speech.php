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

if ( array_key_exists( 'result',   $_REQUEST ) ) {
  $result   = $_REQUEST['result'];
} elseif ( array_key_exists( 'result',   $_SESSION ) ) {
  $result   = $_SESSION['result'];
} else {
  $result   = '';
}

if ( array_key_exists( 'input_type',   $_REQUEST ) ) {
  $input_type   = $_REQUEST['input_type'];
} elseif ( array_key_exists( 'input_type',   $_SESSION ) ) {
  $input_type   = $_SESSION['input_type'];
} else {
  $input_type   = '';
}

if ( $exiting ) {
  header('Content-Type: text/plain');
  print "OK";
  exit();
}

header('Content-Type: text/xml');
$xml->startElement('document');
$xml->writeAttribute('type', 'xml/freeswitch-httapi');

if ($result) {
  $xml->startElement('work');
  
  if ($type == "dtmf") {
    $xml->startElement("say");
    $xml->writeAttribute('language', "en");
    $xml->writeAttribute('type', "name_spelled");
    $xml->writeAttribute('method', "pronounced");
    $xml->text( $result );
  }
  
  $xml->startElement("log");
  $xml->writeAttribute('level', "crit");
  $xml->text($result);
  $xml->endElement();

  $xml->writeElement('hangup');
  $xml->endElement();
} else {
  $xml->startElement('work');

  $xml->startElement('pause');
  $xml->writeAttribute('milliseconds', "1500");
  $xml->endElement();

  $xml->startElement('playback');
  $xml->writeAttribute('name', "result");
  $xml->writeAttribute('asr-engine', "pocketsphinx");
  $xml->writeAttribute('asr-grammar', "pizza_yesno");
  $xml->writeAttribute('file', "http://sidious.freeswitch.org/sounds/ConfirmDelivery.wav");
  $xml->writeAttribute('error-file', "http://sidious.freeswitch.org/sounds/invalid.wav");
  
  $xml->startElement("bind");
  $xml->writeAttribute('strip', "#");
  $xml->text("~\\d+\#");
  $xml->endElement();

  $xml->endElement();
  
  $xml->endElement();
}

$xml->endElement(); // </document>

print $xml->outputMemory();



