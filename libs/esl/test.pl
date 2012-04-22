#!/usr/bin/perl
require ESL;
use Data::Dumper;

my $fd = fileno(STDIN);
my $con = new ESL::ESLconnection($fd);
my $info = $con->getInfo();

select STDERR;

print $info->serialize();

my $uuid = $info->getHeader("unique-id");

$con->execute("answer", "", $uuid);
$con->execute("playback", "/ram/swimp.raw", $uuid);


$con->disconnect();


