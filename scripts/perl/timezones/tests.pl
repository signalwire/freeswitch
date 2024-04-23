#!/usr/bin/perl
=pod
Tests to verify that the provided modifications to timezone formats produce 
the correct results. The first set of tests verify the fixTzstr subroutine 
converts the quoted values to something that won't make FreeSWITCH default to
UTC.

The second set of tests confirms that those timezone changes actually produce
the correct timestamps.

Make sure FreeSWITCH already has already loaded the timezones.conf.xml that you 
want to test.

To run tests:

TIMEZONES_XML_PATH=path/to/timezones.conf.xml prove tests.pl
=cut

use strict;
use warnings;
use Test::More;
use ESL;
use XML::LibXML::Reader;

require "./fix-tzstr.pl";

use Env qw(TIMEZONES_XML_PATH);
die "The TIMEZONES_XML_PATH environment variable must be set to test timezones." unless ( defined($TIMEZONES_XML_PATH) );

ok( fixTzstr("<-02>2", "doesntmatterhere") eq "STD2" );
ok( fixTzstr("EST5EDT,M3.2.0,M11.1.0", "US/Eastern") eq "EST5EDT,M3.2.0,M11.1.0" );
ok( fixTzstr("<+11>-11", "GMT-11") eq "GMT-11" );
ok( fixTzstr("<-02>2<-01>,M3.5.0/-1,M10.5.0/0", "America/Godthab") eq "STD2DST,M3.5.0/-1,M10.5.0/0" );

my $test_count = 4;

my $tz_fmt = "%Y-%m-%d %H:%M:%S";
my $c = new ESL::ESLconnection("127.0.0.1", "8021", "ClueCon");
$c->api("reloadxml")->getBody();
my $epoch = $c->api("strepoch")->getBody();
run_tests($epoch);
run_tests("1699613236"); # testing DST, add more epochs as needed

sub run_tests {
    my $epoch = shift;
    my $reader = XML::LibXML::Reader->new(location => $TIMEZONES_XML_PATH);
    while ($reader->read) {
        my $tag = $reader;
        if ( $tag->name eq "zone" && $tag->hasAttributes() ) {
            my $zn = $tag->getAttribute("name");

            my $cmd = `TZ='$zn' date +'$tz_fmt' --date='\@$epoch'`;
            my $sys_time = $cmd =~ s/\n//r;
            my $fs_time = $c->api("strftime_tz $zn $epoch|$tz_fmt")->getBody();

            ok ( $sys_time eq $fs_time, $zn ) or diag(
                "  (sys) $sys_time\t(fs) $fs_time"
            );

            $test_count++;
        }
    }
}

done_testing($test_count);