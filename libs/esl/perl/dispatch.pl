#!/usr/bin/perl

use ESL::Dispatch;
use Data::Dumper;
my $daemon = init ESL::Dispatch({});

$| = 1;

sub worker {
  my $self = shift;
  print "I'm a worker\n";
}

sub heartbeat {
  my $self = shift;
  my $event = $self->{event_hash};
  print Dumper $event;
}



$daemon->set_worker(\&worker, 2000);
$daemon->set_callback("heartbeat", \&heartbeat);
$daemon->set_callback("channel_hangup", \&heartbeat);


$daemon->run;
