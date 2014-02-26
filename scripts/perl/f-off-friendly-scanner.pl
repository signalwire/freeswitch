#!/usr/bin/perl
# Fsck Friendly Scanner
#
use Data::Dumper;
require ESL;
$| = 1;
my $c = new ESL::ESLconnection("localhost", "8021", "ClueCon");
$c->events("plain", "CUSTOM sofia::register");

while ($c->connected()) {
  my $event = $c->recvEvent(); 

  my $user_agent = $event->getHeader('user-agent');
  my $network_ip = $event->getHeader('network-ip');

  if ($user_agent =~ m/(friendly-scanner|sipcli)/i) {
      system("/sbin/iptables -I INPUT -s $network_ip -j DROP");
  }
}
