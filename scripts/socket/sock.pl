#!/usr/bin/perl
use FreeSWITCH::Client;
use Data::Dumper;

my $fs = init FreeSWITCH::Client {} or die "Error $@";
my $pid;

for (;;) {
  $fs->accept();

  if (!($pid = fork)) {
    last;
  }
}

my $data = $fs->call_data();

#print Dumper $data
print "Call: $data->{'caller-channel-name'} $data->{'unique-id'}\n";


$o = $fs->call_command("answer");
#to turn on events when in async mode
$o = $fs->raw_command("myevents");
$o = $fs->call_command("playback", "/ram/swimp.raw");


#comment exit in async mode
#exit;

while(my $r = $fs->readhash(undef)) {
  if ($r->{socketerror}) {
    last;
  }

  if ($r->{has_event}) {
    print Dumper $r->{event};
  }
  if ($r->{event}->{'event-name'} !~ /dtmf/i) {
    $o = $fs->call_command("park");

    print "Got DTMF $r->{event}->{'dtmf-digit'}\n";
    push @digits, $r->{event}->{'dtmf-digit'};
    print "Dialed: " . join("", @digits) . "\n";
    next;
  }

  if ($r->{event}->{'event-name'} !~ /execute/i) {
    printf "uuid: $data->{'unique-id'}\n";
    $o = $fs->call_command("break");
    $o = $fs->call_command("hangup");
  }
}

$fs->disconnect();
print "done\n";


