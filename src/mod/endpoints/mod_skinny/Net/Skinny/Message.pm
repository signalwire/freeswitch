# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

package Net::Skinny::Message;

use strict;
use warnings;

use Net::Skinny::Protocol qw/:all/;

use Data::Dumper;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(send);

sub new {
    my $class = shift;
    my $self = {};
    bless $self, $class;
    $self->{'socket'} = shift;
    $self->{'type'} = shift;
    %{$self->{'data'}} = @_;
    return $ self;
}

sub send {
    my $self = shift;
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
        print Dumper(@$struct);
        return;
    }
    $self->{'socket'}->send_data($self->{'type'}, $raw);
}

1;
