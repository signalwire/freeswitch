#!/usr/bin/perl

use strict;
use warnings;

my $remote_version = `wget --quiet https://data.iana.org/time-zones/tzdb/version --output-document -` =~ s/\n//r;
my $local_version;

if ( open my $in, "<data/version" ) {
    $local_version = do { local $/; <$in> };
    close $in;
}

my $up_to_date = defined($local_version) && $local_version eq $remote_version;

if ( ! $up_to_date ) {
    open my $out, ">data/version";
    print $out $remote_version;
    close $out;
}

$local_version = $remote_version;

`wget --quiet --timestamping --directory-prefix=data https://data.iana.org/time-zones/tzdb-latest.tar.lz`;
`tar --extract --file=data/tzdb-latest.tar.lz --directory=data`;
`make DESTDIR=../ TZDIR=zones-$local_version --directory=data/tzdb-$local_version posix_only`;

print("Yay. Now you can run\n  ./timezone-gen.pl --base=data/zones-$local_version --output=timezones-$local_version.conf.xml")