#!/usr/bin/perl
  
use warnings;
use strict;
  
use POE qw(Component::Client::TCP Filter::FSSocket);
use Data::Dumper;

my $auth_sent = 0;
my $password = "ClueCon";
  
POE::Component::Client::TCP->new(
          'RemoteAddress' => '127.0.0.1',
          'RemotePort'    => '8021',
          'ServerInput'   => \&handle_server_input,
          'Filter'        => POE::Filter::FSSocket->new(),
);
  
POE::Kernel->run();
exit;
  
  
sub handle_server_input {
          my ($heap,$input) = @_[HEAP,ARG0];
  
          print Dumper $input;
  
  
	if($input->{'Content-Type'} eq "auth/request") {
	$auth_sent = 1;
	$heap->{'server'}->put("auth $password");
	} elsif ($input->{'Content-Type'} eq "command/reply") {
		if($auth_sent == 1) {
			$auth_sent = -1;
  
			#do post auth stuff
			$heap->{'server'}->put("events plain all");
			$heap->{'server'}->put("log");
			$heap->{'server'}->put("api show channels");
		}
	}
}
