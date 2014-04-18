#!/usr/bin/perl

use ESL::Dispatch;
use Data::Dumper;

my $daemon = init ESL::Dispatch({});
my $calls;
# Debug Yo!
#ESL::eslSetLogLevel(7);

$| = 1;

sub worker {
  my $self = shift;
  print "I'm a worker\n";
}

sub register {
  my $self = shift;
  my $event = shift;
  my $esl  = $self->{_esl};
  my $contact = $event->{contact};
  my $profile = $event->{'profile-name'};
  my $callid  = $event->{'call-id'};

  return if $call->{$callid};
  
  $contact =~ s/.*\<(.*)\>.*/{my_call_id=$callid}sofia\/$profile\/$1/;  
  $esl->api('bgapi', "originate $contact &echo");
  print "Adding $callid to calls hash\n";
  $call->{$callid} = 1;
}

sub channel_hangup {
  my $self = shift;
  my $event = shift;
  my $callid = $event->{variable_my_call_id};
  print "Removing $callid from calls hash\n";
  delete $call->{$callid};
}

$0 = "ESL::Dispatch rocks!";

$daemon->set_worker(\&worker, 2000);
$daemon->set_callback("custom", \&register, 'sofia::register');
$daemon->set_callback("channel_hangup", \&channel_hangup);

$daemon->run;
