=head1 NAME

POE::Filter::FSSocket - a POE filter that parses FreeSWITCH events into hashes

=head1 SYNOPSIS

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
          'Filter'        => 'POE::Filter::FSSocket',
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
                  }
          }
  }

=head1 EXAMPLES

See examples in the examples directory of the distribution.

=head1 DESCRIPTION

POE::Filter::FSSocket parses output from FreeSWITCH into hashes.  FreeSWITCH 
events have a very wide range of keys, the only consistant one being
Content-Type.  The keys are dependant on the type of events.  You must use the 
plain event type as that is what the filter knows how to parse.  You can ask for
as many event types as you like or all for everything.  You specify a list of
event types by putting spaces between them ex: "events plain api log talk"

Currently known event types (Event-Name):

  CUSTOM
  CHANNEL_CREATE
  CHANNEL_DESTROY
  CHANNEL_STATE
  CHANNEL_ANSWER
  CHANNEL_HANGUP
  CHANNEL_EXECUTE
  CHANNEL_BRIDGE
  CHANNEL_UNBRIDGE
  CHANNEL_PROGRESS
  CHANNEL_OUTGOING
  CHANNEL_PARK
  CHANNEL_UNPARK
  API
  LOG
  INBOUND_CHAN
  OUTBOUND_CHAN
  STARTUP
  SHUTDOWN
  PUBLISH
  UNPUBLISH
  TALK
  NOTALK
  SESSION_CRASH
  MODULE_LOAD
  DTMF
  MESSAGE
  CODEC
  BACKGROUND_JOB
  ALL

Currently handled FreeSWITCH messages (Content-Type):

  auth/request
  command/response
  text/event-plain
  api/response (data in __DATA__ variable)
  log/data (data in __DATA__ variable)

=cut


package POE::Filter::FSSocket;

use warnings;
use strict;

use Carp qw(carp croak);
use vars qw($VERSION);
use base qw(POE::Filter);

$VERSION = '0.05';

use POE::Filter::Line;
use Data::Dumper;

#self array
sub LINE_FILTER()      {1}
sub PARSER_STATE()     {2}
sub PARSER_STATENEXT() {3}
sub PARSED_RECORD()    {4}
sub CURRENT_LENGTH()   {5}
sub STRICT_PARSE()     {6}
sub DEBUG_LEVEL()      {7}

#states of the parser
sub STATE_WAITING()     {1} #looking for new input
sub STATE_CLEANUP()     {2} #wipe out record separators
sub STATE_GETDATA()     {3} #have header, get data
sub STATE_FLUSH()       {4} #puts us back in wait state and tells us to kill the parsed_record
sub STATE_TEXTRESPONSE() {5} #used for api output

sub new {
	my $class = shift;
	my %args = @_;

	my $strict = 0;
	my $debug  = 0;

	if(defined($args{'debug'})) {
		$debug = $args{'debug'};
	}

	if(defined($args{'strict'}) && $args{'strict'} == 1) {
		$strict = $args{'strict'};
	}

	#our filter is line based, don't reinvent the wheel
	my $line_filter = POE::Filter::Line->new();

	my $self = bless [
		"",            #not used by me but the baseclass clone wants it here
		$line_filter,  #LINE_FILTER
		STATE_WAITING, #PARSER_STATE
		undef,         #PARSER_STATE
		{},            #PARSED_RECORD
		0,             #length tracking (for Content-Length when needed)
		$strict,       #whether we should bail on a bad parse or try and save the session
		$debug,        #debug level
	], $class;

	return $self;
}


sub get_one_start {
	my ($self, $stream) = @_;
	$self->[LINE_FILTER]->get_one_start($stream);
}

sub get_one {
	my $self = shift;

	while(1) {
		#grab a line from the filter
		my $line = $self->[LINE_FILTER]->get_one();

		#quit if we can't get any lines
		return [] unless @$line;

		#get the actual line
		$line = $line->[0];

		if(($self->[PARSER_STATE] == STATE_WAITING) || ($self->[PARSER_STATE] == STATE_FLUSH)) {
			#see if we need to wipe out the parsed_record info
			if($self->[PARSER_STATE] == STATE_FLUSH) {
				delete $self->[PARSED_RECORD];
				$self->[CURRENT_LENGTH] = 0;

				$self->[PARSER_STATE] = STATE_WAITING;
			}

			if($line =~ /Content-Length:\ (\d+)$/) {
				#store the length
				$self->[PARSED_RECORD]{'Content-Length'} = $1;

				#see if we had a place to go from here (we should)
				if(defined($self->[PARSER_STATENEXT])) {
					$self->[PARSER_STATE] = $self->[PARSER_STATENEXT];
					$self->[PARSER_STATENEXT] = undef;
				}
			} elsif($line =~ /Content-Type:\ (.*)$/) {
				#store the type of request
				$self->[PARSED_RECORD]{'Content-Type'} = $1;

				if($1 eq "auth/request") {
					$self->[PARSER_STATE]     = STATE_CLEANUP;
					$self->[PARSER_STATENEXT] = STATE_FLUSH;
					return [ $self->[PARSED_RECORD] ];
				} elsif ($1 eq "command/reply") { #do something with this later
					$self->[PARSER_STATE] = STATE_GETDATA;
				} elsif ($1 eq "text/event-plain") {
					$self->[PARSER_STATE]     = STATE_CLEANUP;
					$self->[PARSER_STATENEXT] = STATE_GETDATA;
				} elsif ($1 eq "api/response") {
					$self->[PARSER_STATENEXT] = STATE_TEXTRESPONSE;
				} elsif ($1 eq "log/data") {
					$self->[PARSER_STATENEXT] = STATE_TEXTRESPONSE;
				} else { #unexpected input
					croak ref($self) . " unknown input [" . $self->[PARSER_STATE] . "] (" . $line . ")";
				}
			} else {
				#already in wait state, if we are not in strict, keep going
				if($self->[STRICT_PARSE]) {
					croak ref($self) . " unknown input [STATE_WAITING] (" . $line . ")";
				}
			}
		} elsif ($self->[PARSER_STATE] == STATE_CLEANUP) {
			if($line eq "") {
				if(defined($self->[PARSER_STATENEXT])) {
					$self->[PARSER_STATE] = $self->[PARSER_STATENEXT];
					$self->[PARSER_STATENEXT] = undef;
				} else {
					$self->[PARSER_STATE] = STATE_WAITING;
				}
			} else {
				#see if we should bail
				if($self->[STRICT_PARSE]) {
					croak ref($self) . " unknown input [STATE_CLEANUP] (" . $line . ")";
				} else {
					#we are not supposed to bail so try and save our session...
					#since we are think we should be cleaning up, flush it all away
					$self->[PARSER_STATE] = STATE_FLUSH;

					#parser fail should be considered critical, if any debug at all, print dump
					if($self->[DEBUG_LEVEL]) {
						print STDERR "Parse failed on ($line) in STATE_CLEANUP:\n";
						print STDERR Dumper $self->[PARSED_RECORD];
					}
				}
			}
		} elsif ($self->[PARSER_STATE] == STATE_GETDATA) {
			if($line =~ /^([^:]+):\ (.*)$/) {
				$self->[PARSED_RECORD]{$1} = $2;
			} elsif ($line eq "") { #end of event 
				$self->[PARSER_STATE] = STATE_FLUSH;

				return [ $self->[PARSED_RECORD] ];
			} else {
				if($self->[STRICT_PARSE]) {
					croak ref($self) . " unknown input [STATE_GETDATA] (" . $line . ")";
				} else {
					#flush and run
					$self->[PARSER_STATE] = STATE_FLUSH;

					#parser fail should be considered critical, if any debug at all, print dump
					if($self->[DEBUG_LEVEL]) {
						print STDERR "Parse failed on ($line) in STATE_GETDATA:\n";
						print STDERR Dumper $self->[PARSED_RECORD];
					}
				}
			}
		} elsif ($self->[PARSER_STATE] == STATE_TEXTRESPONSE) {
			if($self->[CURRENT_LENGTH] == -1) {
				$self->[CURRENT_LENGTH] = 0;
				next;
			}

			$self->[CURRENT_LENGTH] += (length($line) + 1);

			if(($self->[CURRENT_LENGTH] - 1) == $self->[PARSED_RECORD]{'Content-Length'}) {
				$self->[PARSER_STATE] = STATE_FLUSH;
				$self->[PARSED_RECORD]{'__DATA__'} .= $line;

				return [$self->[PARSED_RECORD]];
			} else {
				$self->[PARSED_RECORD]{'__DATA__'} .= $line . "\n";
			}
		}
	}
}

sub put {
	my ($self, $lines)  = @_;

	my @row;
	foreach my $line (@$lines) {
		push @row, $line . "\n\n";
	}

	return \@row;
	
}

sub get_pending {
	my $self = shift;
	return $self->[LINE_FILTER]->get_pending();
}

sub get {
	my ($self, $stream) = @_;
	my @return;

	$self->get_one_start($stream);
	while(1) {
		my $next = $self->get_one();
		last unless @$next;
		push @return, @$next;
	}

	return \@return;
}

1;

=head1 SEE ALSO

FreeSWITCH - http://www.freeswitch.org/

=head1 AUTHORS

POE::Filter::FSSocket is written by Paul Tinsley.  You can reach him by e-mail
at pdt@jackhammer.org.

=head1 COPYRIGHT

Copyright 2006, Paul Tinsley. All rights are reserved.

POE::Filter::FSSocket is free software; it is currently licensed under the MPL
license version 1.1.

=cut
