#!/usr/bin/perl

# analyze-debug-alloc.pl
# generate allocation report by processing log files

# Note that this script is only useful when run against freeswitch log files
# produced when server is running with DEBUG_ALLOC and DEBUG_ALLOC2 set.
# It's purely for diagnosing memory leaks.

use strict;
use JSON;

my $debug = 0;

my @logs = sort glob("freeswitch.log.*");
push( @logs, "freeswitch.log" );

my %pools = ();

foreach my $file (@logs) {
    open( my $in, "<$file" );
    while ( defined( my $line = <$in> ) ) {
        if ( $line =~ /(0x[0-9A-Fa-f]+) DESTROY POOL$/o ) {
            my $paddr = $1;
            if ( !$pools{$paddr} ) {
                $debug && print "WARN: No ref to pool $paddr.\n";
            }
            else {
                foreach my $alloc ( @{ $pools{$paddr}->{allocs} } ) {

                    # debug, might not be needed
                }
                delete $pools{$paddr};
            }
        }
        elsif ( $line =~ /(0x[0-9A-Fa-f]+) Free Pool/o ) {
            my $paddr = $1;
            if ( !$pools{$paddr} ) {
                $debug && print "WARN: No ref to pool $paddr.\n";
            }
            else {
                foreach my $alloc ( @{ $pools{$paddr}->{allocs} } ) {

                    # debug, might not be needed
                }
                delete $pools{$paddr};
            }
        }
        elsif ( $line =~ /(0x[0-9A-Fa-f]+) New Pool (.*)$/o ) {
            my $paddr = $1;
            my $where = $2;
            if ( $pools{$paddr} ) {
                $debug && print "WARN: Duplicate pool $paddr at $where.\n";
            }
            $pools{$paddr}->{where} = $where;
            if ( !$pools{$paddr}->{allocs} ) {
                $pools{$paddr}->{allocs} = [];
            }
        }
        elsif ( $line =~ /CONSOLE\] \s*(.*?:\d+) (0x[0-9A-Fa-f]+) Core Allocate (.*:\d+)\s+(\d+)$/o ) {
            my $where  = $1;
            my $paddr  = $2;
            my $pwhere = $3;
            my $size   = $4;
            if ( !$pools{$paddr} ) {
                $debug && print "WARN: Missing pool ref for alloc of $size from $paddr at $where (pool $pwhere)\n";
            }
            $pools{$paddr}->{where} = $where;
            push( @{ $pools{$paddr}->{allocs} }, { size => $size, where => $where } );
        }
        elsif ( $line =~ /CONSOLE\] \s*(.*?:\d+) (0x[0-9A-Fa-f]+) Core Strdup Allocate (.*:\d+)\s+(\d+)$/o ) {
            my $where  = $1;
            my $paddr  = $2;
            my $pwhere = $3;
            my $size   = $4;
            if ( !$pools{$paddr} ) {
                $debug
                    && print "WARN: Missing pool ref for strdup alloc of $size from $paddr at $where (pool $pwhere)\n";
            }
            $pools{$paddr}->{where} = $where;
            push( @{ $pools{$paddr}->{allocs} }, { size => $size, where => $where } );
        }
    }
}

my $used                = 0;
my $pcount              = 0;
my $acount              = 0;
my %pool_cnt_by_where   = ();
my %alloc_size_by_where = ();
my %alloc_cnt_by_where  = ();
foreach my $pool ( keys %pools ) {
    my $where = $pools{$pool}->{where};
    $pcount++;
    $pool_cnt_by_where{$where}++;

    foreach my $alloc ( @{ $pools{$pool}->{allocs} } ) {
        my $sz    = $alloc->{size};
        my $where = $alloc->{where};

        $acount++;
        $alloc_size_by_where{$where} += $sz;
        $alloc_cnt_by_where{$where}++;

        $used += $sz;
    }
}

print "Used: $used\n";
print "Pool Count: $pcount\n";
print "Alloc Count: $acount\n";

my $json = new JSON;
$json->pretty(1);
$json->canonical(1);

print "Pool Count by Where:\n";
foreach my $pool ( sort { $pool_cnt_by_where{$a} <=> $pool_cnt_by_where{$b} || $a cmp $b } keys %pool_cnt_by_where ) {
    print $pool_cnt_by_where{$pool}, "\t", $pool, "\n";
}
print "\n";

print "Alloc Count by Where:\n";
foreach my $pool ( sort { $alloc_cnt_by_where{$a} <=> $alloc_cnt_by_where{$b} || $a cmp $b } keys %alloc_cnt_by_where )
{
    print $alloc_cnt_by_where{$pool}, "\t", $pool, "\n";
}
print "\n";

print "Alloc Size by Where:\n";
foreach
    my $pool ( sort { $alloc_size_by_where{$a} <=> $alloc_size_by_where{$b} || $a cmp $b } keys %alloc_size_by_where )
{
    print $alloc_size_by_where{$pool}, "\t", $pool, "\n";
}
print "\n";
