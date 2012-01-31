<?php
$xml = new XMLWriter();
$xml->openMemory();
$xml->setIndent(1);
$xml->startDocument();

if ( $_REQUEST['exiting'] ) {
  header('Content-Type: text/plain');
  print "OK";
  exit();
}

header('Content-Type: text/xml');
$xml->startElement('document');
$xml->writeAttribute('type', 'xml/freeswitch-httapi');

$xml->startElement('work');

$xml->startElement('pause');
$xml->writeAttribute('milliseconds', '1500');
$xml->endElement(); // </pause>

$xml->startElement('playback');
$xml->writeAttribute('name', 'digits');
$xml->writeAttribute('file', 'http://sidious.freeswitch.org/sounds/exten.wav');
$xml->writeAttribute('error-file', 'http://sidious.freeswitch.org/sounds/invalid.wav');
$xml->writeAttribute('input-timeout', '5000');
$xml->writeAttribute('action', 'dial:default:XML');

$xml->startElement("bind");
$xml->writeAttribute('strip',"#");
$xml->text("~\\d+\#");
$xml->endElement(); // </bind>
$xml->endElement(); // </playback>

$xml->endElement(); // </work>
$xml->endElement(); // </document>

print $xml->outputMemory();



