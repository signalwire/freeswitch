#!/usr/bin/perl
#
# see http://developer.whitepages.com/ for details
#

use Data::Dumper;
use Net::WhitePages;
use CGI qw/:standard/;

my $wp = Net::WhitePages->new(TOKEN => "API_TOKEN");


$caller_id = param('caller_id');

print header("text/plain");
my $res = $wp->reverse_phone(phone => $caller_id);
foreach $entry (@{$res->{listings}}) {
  if($entry->{business}) {
    print uc("$entry->{business}->{businessname}");
  } elsif ($entry->{displayname}) {
    print uc("$entry->{displayname}");
  } else {
    print uc("$entry->{address}->{city}, $entry->{address}->{state}");
  }
  last;
};
