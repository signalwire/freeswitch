#!/usr/bin/perl

use strict;
use Getopt::Long;

my $base   = "/usr/share/zoneinfo";
my $output = "timezones.conf.xml";
my $debug;
my $help;

my %zones        = ();
my %name_to_file = ();

my $res = GetOptions(
    "base=s" => \$base,
    "debug+" => \$debug,
    "help"   => \$help,
    "output" => \$output
);
if ( !$res || $help ) {
    print "$0 [--base=/usr/share/zoneinfo] [--output=timezones.conf.xml] [--debug] [--help]\n";
    exit;
}

my @dirs = ($base);

while (@dirs) {
    my $dir = shift @dirs;

    opendir( my $top, $dir );
    while ( my $file = readdir($top) ) {
        next if ( $file eq "." || $file eq ".." );

        if ( -f "$dir/$file" ) {
            $debug && print "Found $dir/$file\n";

            my $name = "$dir/$file";
            $name =~ s|^${base}/||o;

            $name_to_file{$name} = "$dir/$file";
        }
        elsif ( -d "$dir/$file" ) {
            $debug && print "Found subdir $dir/$file\n";
            push( @dirs, "$dir/$file" );
        }
    }
    closedir($top);
}

foreach my $name ( sort( keys(%name_to_file) ) ) {
    my $file = $name_to_file{$name};
    $debug && print "Processing $file...\n";

    open( my $in, "<$file" );
    my $data = join( "", <$in> );
    close($in);

    if ( $data !~ /^TZif/o ) {
        $debug && print "Skipped $file\n";
        next;
    }

    my $tmp = $data;
    $tmp =~ s/\n$//s;
    $tmp =~ s/.*\n//sgmo;

    $zones{$name} = $tmp;
}

open( my $out, ">$output" );
print $out "<configuration name=\"timezones.conf\" description=\"Timezones\">\n";
print $out " " x 4, "<timezones>\n";

my $lastprefix = "";
foreach my $zone ( sort( keys(%zones) ) ) {
    my $str = $zones{$zone};
    next if ( !$str );

    my $newprefix = $zone;
    $newprefix =~ s|/.*||go;
    if ( $newprefix ne $lastprefix && $lastprefix ne "" ) {
        print $out "\n";
    }
    $lastprefix = $newprefix;

    print $out "\t<zone name=\"$zone\" value=\"$str\" />\n";
}
print $out " " x 4, "</timezones>\n";
print $out "</configuration>\n";
close($out);
