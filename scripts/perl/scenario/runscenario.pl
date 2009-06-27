#!/usr/bin/perl
#
#  Scenario Test Execution.
#
use LWP::UserAgent;
use Data::Dumper;

$| =1;

our $ua = LWP::UserAgent->new;
my $phone = load_config();

if(-f $ARGV[0]) {
  run_scenario($ARGV[0]);
} else {
  print "No Scenario File?\n";
  exit;
}

sub run_scenario($$) {
  $file = shift;
  open(SCENARIO,"<$file");
  @commands = <SCENARIO>;
  print Dumper $info;
  foreach $command (@commands) {
    chomp $command;
    my($target, $type, $button, $delay) = split(",",$command);
    &push_button($phone->{$target}, "$type", "$button", $delay);

  }
}

sub push_button ($$$) {
  $info = shift;
  $type = shift;
  $button = shift;
  $delay = shift;

  if($delay) {
    sleep($delay);
  } else {
    $delay = 0;
  }
  print "$info->{name} -> $type => $button with delay $delay\n";

  $request = HTTP::Request->new("GET", "http://$info->{ip}/command.htm?$type=$button");
  $return = $ua->request($request);
}

sub load_config {
  open(CFG,"<phones.cfg");
  @phones = <CFG>;
  foreach $line (@phones) {
    chomp $line;
    my($name,$ip,$extension) = split(",", $line);
    $phone->{$name} = {name => $name, ip => $ip, extension => $extension}
  }
  return $phone;
}
