#!/usr/bin/perl -w
use strict;

=head1 NAME

combineconf.pl - expand #include PIs in a freeswitch conf file

=head1 SYNOPSIS

 # cd conf
 # ../scripts/combineconf.pl freeswitch.xml > freeswitch_combined.xml

=head1 DESCRIPTION

This is recursive, and will take multiple input files on the command line.

You need to run it from the working directory that the relative include paths
except to be resolved from.

=head1 AUTHOR

Mark D. Anderson (mda@discerning.com)
Released under same terms as Perl, or alternatively the MPL.

=cut

use IO::File;

sub filter_file {
    my ($f) = @_;
    my $fh = $f eq '-' ? \*STDIN : IO::File->new($f, 'r');
    die "ERROR: Can't open $f: $!\n" unless $fh;
    while(<$fh>) {
	if (m/<!--#include\s+"(.*?)"/) {
	    filter_file($1);
	} 
	else {print;}
    }
    undef $fh;
}

sub main {
    die "Usage: $0 file1 ...\nCombined output goes to stdout. Use '-' as the filename to use stdin." unless @ARGV;
    for(@ARGV) {
	filter_file($_);
    }
}

main();
