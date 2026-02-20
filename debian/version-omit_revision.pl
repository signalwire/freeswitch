#!/usr/bin/perl
use strict;
use warnings;
use Dpkg::Version;

my $version;

open(my $fh, '-|', 'dpkg-parsechangelog -S version') or die "Failed to execute dpkg-parsechangelog: $!";
{
    local $/;
    $version = <$fh>;
}
close $fh;

$version =~ s/\s+$//;

die "No version found or empty output from dpkg-parsechangelog" unless defined $version and $version ne '';

my $v = Dpkg::Version->new($version);
my $vs = $v->as_string(omit_epoch => 1, omit_revision => 1);

print "$vs\n";
