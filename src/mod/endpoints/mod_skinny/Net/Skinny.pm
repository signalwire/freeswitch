# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

package Net::Skinny;

use strict;
use warnings;

require IO::Socket;

use Net::Skinny::Protocol qw/:all/;

our @ISA = qw(IO::Socket::INET);

sub new {
    shift->SUPER::new(PeerPort => 2000, @_);
}

sub send_raw
{
    my $self = shift;
	my $type = shift;
	my $raw = shift;
	my $len = length($raw)+4;
	printf "Sending message (length=%d, type=%s (%X))", $len, Net::Skinny::Protocol::skinny_message_type2str($type), $type;
	$self->send(pack("VVV", $len, 0, $type).$raw);
	printf ".\n";
}

sub send_message
{
    my $self = shift;
    my $type = shift;
    my $message = Net::Skinny::Message->new($type, @_);
    return $self->send_raw($message->type(), $message->raw());
}

sub receive_message
{
    my $self = shift;
	my $buf;
	$self->recv($buf, 4);
	my $len = unpack("V", $buf);
	printf "Receiving message (length=%d,", $len;
	if($len < 4) {
		printf "type=?).\n";
		printf "Problem! Length is < 4.\n";
		exit 1;
	}
	$self->recv($buf, 4); #reserved
	$self->recv($buf, 4); #type
	my $type = unpack("V", $buf);
	printf "type=%s (%X))", Net::Skinny::Protocol::skinny_message_type2str($type), $type;
	if($len > 4) {
		$self->recv($buf, $len-4);
	} else {
		$buf = '';
	}
	printf ".\n";
	return Net::Skinny::Message->new_raw($type, $buf);
}

sub sleep
{
    my $self = shift;
	my $t = shift;
	my %args = @_;
	$args{'quiet'} = 0 if not $args{'quiet'};
	printf "Sleeping %d seconds", $t;
	while(--$t){
		sleep(1);
		if(!$args{'quiet'}) {
			printf "." if $t % 10;
			printf "_" unless $t % 10;
		}
	}
	printf ".\n";
}

1;
