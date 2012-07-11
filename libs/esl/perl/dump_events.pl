#!/usr/bin/perl
require ESL;

my $command = shift;
my $args = join(" ", @ARGV);

my $con = new ESL::ESLconnection("localhost", "8021", "ClueCon");
$con->events("plain","all");
while ( $con->connected() ) {
    my $e = $con->recvEventTimed(0);
    print $e->serialize;
}
