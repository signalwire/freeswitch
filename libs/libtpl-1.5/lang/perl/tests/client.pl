#!/usr/bin/perl

use strict;
use warnings;

use IO::Socket::INET;
use lib "..";
use Tpl;

our $port = 2000;

# construct tpl
my $i;
my $tpl = Tpl->tpl_map("A(i)",\$i);
$tpl->tpl_pack(1) while ($i=shift @ARGV);
my $request = $tpl->tpl_dump();

# send to server, get response
my $socket = IO::Socket::INET->new("localhost:$port") or die "can't connect";
print $socket $request;
shutdown($socket,1);	    # done writing (half-close)
undef $/;
my $response = <$socket>;	# get reply (slurp)

# decode response (or print error)
my $total;
my $tpl2 = Tpl->tpl_map("i", \$total);
eval { $tpl2->tpl_load(\$response); };
die "invalid response\n" if $@;
$tpl2->tpl_unpack(0);
print "total is $total\n";
