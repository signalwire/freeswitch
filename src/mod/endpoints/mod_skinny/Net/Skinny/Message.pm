# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

package Net::Skinny::Message;

use strict;
use warnings;

use threads;
use threads::shared;

use Net::Skinny::Protocol qw/:all/;

sub new_empty {
    my $class = shift;
    my $self = {};
    bless $self, $class;
    $self->{'type'} = undef;
    $self->{'data'} = undef;
    $self->{'raw'} = undef;
    return $self;	
}

sub new {
	my $self = shift->new_empty();
	$self->type(shift);
	$self->data(@_) if @_;
    return $self;
}

sub new_raw {
	my $self = shift->new_empty();
	$self->type(shift);
	$self->raw(shift);
    return $self;
}

sub type
{
    my $self = shift;
    my $type = @_ ? shift : undef;
    if(defined($type)) {
    	$self->{'type'} = $type;
    }
    return $self->{'type'};
}

sub data
{
    my $self = shift;
    my @data = @_;
    if(@data) {
    	%{$self->{'data'}} = @data;
    	$self->{'raw'} = undef;
    } elsif(!defined($self->{'data'})) {
    	printf "Conversion from raw to data not implemented\n";
    }
    return $self->{'data'};
}

sub raw
{
    my $self = shift;
    my $raw = shift || undef;
    if(defined($raw)) {
    	$self->{'raw'} = $raw;
    	$self->{'data'} = undef;
    }
    if(!defined($self->{'raw'})) {
		my $struct = Net::Skinny::Protocol::skinny_message_struct($self->{'type'});
		my $raw = '';
		my $parsed_count = 0;
		for my $info ( @$struct) {
		    last if !defined($self->{'data'}{@$info[1]});
		    if(@$info[0] eq 'char') {
		        $raw .= pack("a".@$info[2], $self->{'data'}{@$info[1]});
		    } elsif(@$info[0] eq 'uint32_t') {
		        $raw .= pack("V".@$info[2], $self->{'data'}{@$info[1]});
		    } elsif(@$info[0] eq 'uint16_t') {
		        $raw .= pack("n".@$info[2], $self->{'data'}{@$info[1]});
		    } elsif(@$info[0] eq 'struct in_addr') {
		        $raw .= pack("V".@$info[2], $self->{'data'}{@$info[1]});
		    } elsif(@$info[0] eq 'struct station_capabilities') {
		        $raw .= $self->{'data'}{@$info[1]};
		    } else {
		        printf "Unknown type: %s\n", @$info[0];
		        return;
		    }
		    $parsed_count++;
		}
		if($parsed_count != scalar(keys %{$self->{'data'}})) {
			printf "Incomplete message (type=%s (%X)) %d out of %d\n", Net::Skinny::Protocol::skinny_message_type2str($self->{'type'}), $self->{'type'},
		        $parsed_count, scalar(keys %{$self->{'data'}});
		    return;
		}
		$self->{'raw'} = $raw;
    }
    return $self->{'raw'};
}

1;
