#!/usr/bin/perl -w


#brief      Test module avmd by calling all voicemails available
#           in avmd test suite and print detection results to the console.
#author     Piotr Gregor <piotrgregor@rsyncme.org>
#details    If you are testing locally - remember to set avmd to inbound mode,
#           "avmd set inbound" in fs_cli.
#date       15 Sept 2016 03:00 PM


$|++;   # turn on autoflush
use strict;
use warnings;
require ESL;
use POSIX;
use Time::HiRes;


# Hashtable of <destination number : test result expectation> pairs
my %numbers = (
    400 => "DETECTED",
    401 => "DETECTED",
    402 => "DETECTED",
    403 => "DETECTED",
    404 => "DETECTED",
    405 => "DETECTED",
    406 => "DETECTED",
    407 => "DETECTED",
    408 => "DETECTED",
    409 => "DETECTED",
    410 => "DETECTED",
    411 => "DETECTED",
    412 => "DETECTED",
    413 => "DETECTED",
    414 => "DETECTED",
    500 => "NOTDETECTED",
    501 => "NOTDETECTED"
);

my $host = "127.0.0.1";
my $port = "8021";
my $pass = "ClueCon";
my $extension_base = "sofia/internal/1000\@192.168.1.60";

my $playback = 'local_stream://moh';
my $context = 'default'; 
my $endpoint;
my $dest;
my $expectation;
my $callerid;
my $passed = 0;
my $failed = 0;
my $hanguped = 0;


if ($#ARGV + 1 eq 1) {
    $callerid = $ARGV[0];
    print "\nDialing as [" .$callerid ."]\n";
} elsif ($#ARGV + 1 > 1) {
    die "Please specify single caller id.\n";
} else {
    die "Please specify caller id.\n";
}


print "Connecting...\t";
my $con  = new ESL::ESLconnection($host, $port, $pass);
if (!$con) {
    die "Unable to establish connection to $host:$port\n";
}
if ($con->connected()) {
    print "OK.\n";
} else {
    die "Conenction failure.\n";
}

print "Subscribing to avmd events...\t";
$con->events("plain", "CUSTOM avmd::start");
$con->events("plain", "CUSTOM avmd::stop");
$con->events("plain", "CUSTOM avmd::beep");
$con->events("plain", "CHANNEL_CALLSTATE");
$con->events("plain", "CHANNEL_HANGUP");
print "OK.\n\n";
printf("\nRunning [" .keys(%numbers) ."] tests.\n\n");

printf("outbound uuid | destination number | timestamp | expectation | test result\n\n");
foreach $dest (sort keys %numbers) {
    if (!$con->connected()) {
        last;
    }
    $expectation = $numbers{$dest};
    test_once($dest, $callerid, $expectation);
}
print "Disconnected.\n\n";
if (($failed == 0) && ($hanguped == 0)) {
    printf("\n\nOK. All PASS [%s]\n\n", $passed);
} else {
    printf("PASS [%s], FAIL [%s], HANGUP [%s]\n\n", $passed, $failed, $hanguped);
}

sub test_once {
    my ($dest, $callerid, $expectation) = @_;
    my $originate_string =
    'originate ' .
    '{ignore_early_media=true,' .
    'origination_uuid=%s,' . 
    'originate_timeout=60,' .
    'origination_caller_id_number=' . $callerid . ',' .
    'origination_caller_id_name=' . $callerid . '}';
    my $outcome;
    my $result;
    my $event_uuid;
    my $uuid_in = "";

    if(defined($endpoint)) {
        $originate_string .= $endpoint;
    } else {
        $originate_string .= 'loopback/' . $dest . '/' . $context;
    }
    $originate_string .=  ' ' . '&playback(' . $playback . ')';

    my $uuid_out = $con->api('create_uuid')->getBody();
    my ($time_epoch, $time_hires) = Time::HiRes::gettimeofday();

    printf("[%s] [%s]", $uuid_out, $dest);
    $con->bgapi(sprintf($originate_string, $uuid_out));

    while($con->connected()) {
        my $e = $con->recvEvent();
        if ($e) {
            my $event_name = $e->getHeader("Event-Name");
            if ($event_name eq 'CUSTOM') {
                my $avmd_event_type = $e->getHeader("Event-Subclass");
                if ($avmd_event_type eq 'avmd::start') {
                    $uuid_in = $e->getHeader("Unique-ID");
                } elsif (!($uuid_in eq "") && (($avmd_event_type eq 'avmd::beep') || ($avmd_event_type eq 'avmd::stop'))) {
                    $event_uuid = $e->getHeader("Unique-ID");
                    if ($event_uuid eq $uuid_in) {
                        $outcome = $e->getHeader("Beep-Status");
                        if ($outcome eq $expectation) {
                            $result = "PASS";
                            $passed++;
                        } else {
                            $result = "FAIL";
                            $failed++;
                        }
                        last;
                    }
                }
            } elsif ($event_name eq 'CHANNEL_HANGUP') {
                $event_uuid = $e->getHeader("variable_origination_uuid");
                if ($event_uuid eq $uuid_out) {
                    $outcome = "HANGUP";
                    $result = "HANGUP";
                    $hanguped++;
                    last;
                }
            }
        }
    }
    printf("\t[%s]\t[%s]\t\t[%s]\n", POSIX::strftime('%Y-%m-%d %H:%M:%S', localtime($time_epoch)), $expectation, $result);
}
