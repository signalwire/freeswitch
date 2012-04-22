#!/usr/bin/perl
#
# Snom PNP Daemon that can provide URL for provisioning.
#
# Authors: (and wish lists)
#
# Brian West <brian@freeswitch.org> http://www.amazon.com/gp/registry/wishlist/1BWDJUX5LYQE0
# Raymond Chandler <intralanman@freeswitch.org> http://www.amazon.com/gp/registry/wishlist/27XDISBBI4NOU
#
#

use Data::Dumper;
use Net::SIP;
use IO::Socket::Multicast;
use Getopt::Std;

my $count = 0;

getopt("dui");

$| = 1;

if (!$opt_u && !$opt_i) {
  print "Usage: $0 -i <ipaddress> -u <url> [-d 1]\n";
  exit;
}

my $local_addr = $opt_i;
my $local_port = '8160';


sub reply($;) {
  my ($body) = shift;

  if($opt_d) {
    print Dumper $body;
  }

  if($body =~ m/^SUBSCRIBE/i) {
    my $pkt = Net::SIP::Request->new( $body );

    my $resp = $pkt->create_response(200, "OK");
    my $contact = $pkt->get_header('contact');
    $contact =~ s/<sip:(.*)>/$1/i;
    my $leg = Net::SIP::Leg->new(
				 addr => $local_addr,
				 port => $local_port,
				);
    $leg->deliver( $resp, "$contact" );
    
    my $hash = {};


    my @version = split(";", $pkt->get_header('event'));
    foreach my $blah (@version) {
      if($blah =~ /=/) {
	my($var,$val) = split(/=/,$blah);
	$val =~ s/\"//g;
	$hash->{$var} = $val;
      }
    }

    my $prov_url = "$opt_u/{mac}.xml";
    print "Sending pnp provisioning URL as $opt_u/{mac}.xml\n";

    $notify = Net::SIP::Request->new('NOTIFY', $contact, {});
    $notify->set_header('From' => $pkt->get_header('to'));
    $notify->set_header('To' => $pkt->get_header('to'));
    $notify->set_header('User-Agent' => 'test');
    $notify->set_header('Event' => $pkt->get_header('event'));
    $notify->set_header('Contact' => "<sip:$local_host:$local_port>");
    $notify->set_header('Call-ID' => rand());
    $notify->set_header('CSeq' => '1 NOTIFY');
    $notify->set_body("$prov_url");
    $leg->deliver($notify, $contact);
  }
}

my $socket = IO::Socket::Multicast->new(
					LocalPort => '5060',
					LocalAddr => '224.0.1.75',
					Proto => 'udp',
					ReuseAddr => 1
				       );

$socket->mcast_add('224.0.1.75');

while($socket->recv($data,8192)) {
  &reply($data);
}
