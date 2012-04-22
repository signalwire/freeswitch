# Copyright (c) 2010 Mathieu Parent <math.parent@gmail.com>.
# All rights reserved.  This program is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.

package Net::Skinny::Protocol;

use strict;
no strict "refs";
use warnings;
use Carp;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(skinny_message_type2str skinny_message_struct);

my %const;
my %sub;
my %struct;

my $skinny_protocol_h = 'src/mod/endpoints/mod_skinny/skinny_protocol.h';

sub import {
    shift;
    my $callpkg = caller(0);
    _find(@_);
    my $oops;
    my $all = grep /:all/, @_;
    foreach my $sym ($all ? keys %sub : @_) {
        if (my $sub = $sub{$sym}) {
            *{$callpkg . "::$sym"} = $sub;
        }
        else {
            ++$oops;
            carp(qq["$sym" is not exported by the Net::Skinny::Protocol module]);
        }
    }
    croak("Can't continue after import errors") if $oops;
}

sub _find {
    my $fh;
    unless (open($fh, $skinny_protocol_h)) {
        print STDERR "Can't open $skinny_protocol_h: $!\n";
        return;
    }
    while(<$fh>) {
        if( /^#define\s+([\w_]+)\s+(0x[0-9a-f]+)\s*$/i) {
            my ($name, $value) = ($1,hex($2));
            $sub{$name} = sub () { $value };
            $const{$name} = $value;
        } elsif(/^\s*struct\s+PACKED\s+([a-z_]+)\s*\{\s*$/) {
            my $struct_name = $1;
            $struct{$struct_name} = [];
            while(<$fh>) {
                if(/^\s*\}\s*;\s*$/) {
                    last;
                } elsif(/^\s*(((struct)\s+)?([a-z_0-9]+))\s+([a-z_0-9]+)(\[([0-9A-Z_]+)\])?\s*;?\s*(\/\*.*\*\/)?\s*$/) {
                    my $var_name = $1;
                    my $var_type = $5;
                    my $var_size = $7;
                    $var_size = 1 if !defined($var_size);
                    push @{$struct{$struct_name}}, [$var_name, $var_type, $var_size];
                } elsif(/^\s*union\s*skinny_data data;\s*$/) {
                    # union
                } elsif(/^\s*(\/\*.*\*\/)?\s*$/) {
                    # Simple comment
                } else {
                    printf "Unparsed line '%s' in %s\n", $_, $struct_name;
                }
            }
        }
    }
    @sub{@_};
}

sub skinny_message_type2str {
    my $message_type = shift;
    return "UndefinedTypeMessage" if !defined($message_type);
    
    keys %const;
    while (my ($key, $value) = each %const) {
        if($value == $message_type) {
            return $key;
        }
    }
    return "UnknownMessage";
}

sub skinny_message_struct {
    my $message_type = shift;
    my $struct_name = lc(Net::Skinny::Protocol::skinny_message_type2str($message_type));
    return $struct{$struct_name};
}

