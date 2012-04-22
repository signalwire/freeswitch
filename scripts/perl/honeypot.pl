#!/usr/bin/perl
#
# Add this to conf/dialplan/public but only if you wish to setup a honeypot.
#
#   <X-PRE-PROCESS cmd="exec" data="$${base_dir}/bin/honeypot.pl"/>
#

use Data::Dumper;
use LWP::Simple;

# http://www.infiltrated.net/voipabuse/numberscalled.txt

my @numberscalled = split(/\n/, get("http://www.infiltrated.net/voipabuse/numberscalled.txt"));

foreach $number (@numberscalled) {
  my ($num,$ts) = split(/\t/, $number);

  print "<extension name=\"$num\">\n";
  print "  <condition field=\"destination_number\" expression=\"^$num\$\">\n";
  print "    <action application=\"answer\"/>\n";
  print "    <action application=\"sleep\" data=\"30000\"/>\n";
  print "  </condition>\n";
  print "</extension>\n";
}


