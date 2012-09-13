#!/usr/bin/perl

use strict;
use warnings;

use IO::Socket::INET;
use lib "..";
use Tpl;

$SIG{CHLD} = "IGNORE"; # don't create zombies

our $port = 2000;

sub handle_client {
    my $client = shift;

    undef $/;
    my $request = <$client>; # get request (slurp) 

    # read input array, and calculate total
    my ($i,$total);
    my $tpl = Tpl->tpl_map("A(i)", \$i);
    eval { $tpl->tpl_load(\$request); };
    die "received invalid tpl" if $@;
    $total += $i while $tpl->tpl_unpack(1) > 0;

    # formulate response and send
    my $tpl2 = Tpl->tpl_map("i", \$total);
    $tpl2->tpl_pack(0);
    my $response = $tpl2->tpl_dump();
    print $client $response;
    close $client;
}

my $server = IO::Socket::INET->new(LocalPort => $port,
                   Type => SOCK_STREAM,
                   Reuse => 1,
                   Listen => 10 )
        or die "Can't listen on port $port: $!\n";

while (1) {
    my $client = $server->accept();
    next unless $client;
    # new connection
    my $pid = fork;
    die "can't fork: $!\n" unless defined $pid;
    if ($pid > 0) {
        #p arent
        close $client;
    } elsif ($pid == 0) {
        # child
        handle_client($client);
        exit(0);
    }
}
close ($server);
