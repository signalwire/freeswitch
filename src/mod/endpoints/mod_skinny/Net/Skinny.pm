# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

package Net::Skinny;

use strict;
use warnings;
use IO::Socket;
use Net::Skinny::Protocol qw/:all/;

our(@ISA);
@ISA = qw(IO::Socket::INET);

sub new {
    shift->SUPER::new(PeerPort => 2000, @_);
}

sub send_data
{
    my $self = shift;
	my $type = shift;
	my $data = shift;
	my $len = length($data)+4;
	printf "Sending message (length=%d, type=%s (%X))", $len, Net::Skinny::Protocol::skinny_message_type2str($type), $type;
	$self->send(
		pack("VVV", $len, 0, $type).
		$data);
	printf ".\n";
}

sub send_message
{
    my $self = shift;
    my $type = shift;
    return Net::Skinny::Message->new(
        $self,
        $type,
        @_
    )->send();
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
	}
	printf ".\n";
}

sub sleep
{
    my $self = shift;
	my $t = shift;
	
	printf "Sleeping %d seconds", $t;
	while(--$t){
		sleep(1);
		printf "." if $t % 10;
		printf "_" unless $t % 10;
	}
	printf ".\n";
}

1;
