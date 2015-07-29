#!/usr/bin/perl
require ESL;

my $command = shift;
my $args = join(" ", @ARGV);

my $con = new ESL::ESLconnection("127.0.0.1", "8021", "ClueCon");
my $e = $con->api($command, $args);
print $e->getBody();
