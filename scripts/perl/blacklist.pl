#!/usr/bin/perl
#
# Add this to acl.conf.xml
#
#   <X-PRE-PROCESS cmd="exec" data="$${base_dir}/bin/blacklist.pl"/>
#

use Data::Dumper;
use LWP::Simple;

# http://www.infiltrated.net/voipabuse/addresses.txt
# http://www.infiltrated.net/voipabuse/netblocks.txt


my @addresses = split(/\n/, get("http://www.infiltrated.net/voipabuse/addresses.txt"));
my @netblocks = split(/\n/, get("http://www.infiltrated.net/voipabuse/netblocks.txt"));

print "<list name=\"voip-abuse-addresses\" default=\"allow\">\n";
foreach $addr (@addresses) {
  print "  <node type=\"deny\" cidr=\"$addr/32\"/>\n";
}
print "</list>\n";


print "<list name=\"voip-abuse-netblocks\" default=\"allow\">\n";
foreach $netb (@netblocks) {
  print "  <node type=\"deny\" cidr=\"$netb\"/>\n";
}
print "</list>\n";
