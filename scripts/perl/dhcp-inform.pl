#!/usr/bin/perl
#
# Send DHCPACK when you receive a DHCPINFORM from a polycom 
# so we can provide the provisioning URL to the phone
#
# Authors: (and wish lists)
#
# Brian West <brian@freeswitch.org> http://www.amazon.com/gp/registry/wishlist/1BWDJUX5LYQE0
# Raymond Chandler <intralanman@freeswitch.org> http://www.amazon.com/gp/registry/wishlist/27XDISBBI4NOU
#
#

use IO::Socket::INET;
use Net::DHCP::Packet;
use Net::DHCP::Constants;
use Getopt::Std;

getopt("du");

$| = 1;

if (!$opt_u) {
  print "Usage: $0 -u <url> [-d 1]\n";
  exit;
}

$sock = IO::Socket::INET->new(
			      LocalPort => '67',
			      Proto => 'udp',
			      Broadcast => 1,
			      ReuseAddr => 1,
			     ) or die "socket: $@";

while ($sock->recv($newmsg, 1024)) {
  my $dhcpreq = Net::DHCP::Packet->new($newmsg);
  if ($opt_d) {
    print $dhcpreq->toString();
    print "\n---------------------------------------------------------------------\n";
  }
  $tmp = $dhcpreq->chaddr();

  print "--$tmp--\n";

  if ($dhcpreq->getOptionValue(DHO_DHCP_MESSAGE_TYPE()) == 8 && $dhcpreq->chaddr() =~ /^0004f2/) {
    my $dhcpresp = new Net::DHCP::Packet(
					 Op => BOOTREPLY(),
					 Hops => $dhcpreq->hops(),
					 Xid => $dhcpreq->xid(),
					 Htype => $dhcpreq->htype(),
					 Ciaddr => $dhcpreq->ciaddr(),
					 Chaddr => $dhcpreq->chaddr(),
					 DHO_DHCP_MESSAGE_TYPE() => DHCPACK(),
					 DHO_DHCP_SERVER_IDENTIFIER() => $sock->sockhost,
					 DHO_TFTP_SERVER() => "$opt_u",
					);


    if ($opt_d) {
      print $dhcpresp->toString();
      print "\n---------------------------------------------------------------------\n";
    }
    print "Sending option 66 as $opt_u\n";

    $handle = IO::Socket::INET->new(Proto => 'udp',
				    PeerPort => '68',
				    LocalPort => '67',
				    ReuseAddr => 1,
				    PeerAddr => $dhcpreq->ciaddr(),
				   ) or die "socket: $@";
    $handle->send($dhcpresp->serialize())
  }
}
