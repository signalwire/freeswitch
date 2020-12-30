#!/usr/bin/perl -w


#brief  Make multiple calls. This script can be used to check CPU usage on calls running avmd (see test.txt).
#author Piotr Gregor <piotr@dataandsignal.com>
#date   15 Sept 2016 02:44 PM
#changed 27th Dec 2019


use strict;
use warnings;
require ESL;
use POSIX;
use Time::HiRes;

my $host = "127.0.0.1";
my $port = "8021";
my $pass = "ClueCon";
my $extension_base = "sofia/internal/1000\@192.168.1.1";

my $playback = 'local_stream://moh';
my $context = 'default'; 
#Example:
#my $endpoint = "originate {originator_codec=PCMA,origination_uuid=%s}sofia/gateway/box_b/840534002  \&park()";
#my $endpoint = "originate {originator_codec=PCMA,origination_uuid=%s}sofia/gateway/%s/%s  \&park()";
my $endpoint;
my $gateway;
my $dest;
my $callerid;
my $thread_n;
my $idx = 0;


if ($#ARGV + 1 eq 3) {
    $dest = $ARGV[0];
    $callerid = $ARGV[1];
    $thread_n = $ARGV[2];
    print "Dialing [" .$thread_n ."] calls simultaneously to[" .$dest ."] as [" .$callerid ."]\n";
} else {
    die "Please specify destination number, caller id and number of calls to make\n\nExample:\n./avmd_originate_multiple.pl EXTENSION CALLER NUMBER_OF_CALLS";
}

my $con  = new ESL::ESLconnection($host, $port, $pass);
if (!$con) {
    die "Unable to establish connection to $host:$port\n";
}
if ($con->connected()) {
    print "OK, Connected.\n";
} else {
    die "Connection failure.\n";
}

while($con->connected() && ($idx < $thread_n)) {
    call_once($dest, $callerid, $idx);
    $idx++;
    Time::HiRes::sleep(0.11);    # avoid switch_core_session.c:2265 Throttle Error! 33, switch_time.c:1227 Over Session Rate of 30!
}

print "Disconnected.\n\n";

sub call_once {
    my ($dest, $callerid, $idx) = @_;
    my $originate_string =
    'originate ' .
    '{ignore_early_media=true,' .
    'originator_codec=PCMA,' .
    'origination_uuid=%s,' . 
    'originate_timeout=60,' .
    'origination_caller_id_number=' . $callerid . ',' .
    'origination_caller_id_name=' . $callerid . '}';

    if(defined($endpoint)) {
        $originate_string = '';
        $originate_string .= $endpoint;
    } else {
        $originate_string .= 'loopback/' . $dest . '/' . $context;
        $originate_string .=  ' ' . '&playback(' . $playback . ')';
    }

    my $uuid = $con->api('create_uuid')->getBody();
    my ($time_epoch, $time_hires) = Time::HiRes::gettimeofday();
    printf("[%s]\tCalling with uuid [%s] [%s]... [%s]\n", $idx + 1, $uuid, POSIX::strftime('%Y-%m-%d %H:%M:%S', localtime($time_epoch)), $originate_string);

    $con->bgapi(sprintf($originate_string, $uuid));
    $con->api('uuid_setvar ' . $uuid .' execute_on_answer avmd_start');
}
