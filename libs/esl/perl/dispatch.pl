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

sub channel_hangup {
  my $self = shift;
  my $event = $self->{event_hash}; 
  print "DO SQL GOODIES HERE!\n";
}

$0 = "ESL::Dispatch rocks!";

$daemon->set_worker(\&worker, 2000);
$daemon->set_callback("heartbeat", \&heartbeat);
$daemon->set_callback("channel_hangup", \&channel_hangup);

$daemon->run;
