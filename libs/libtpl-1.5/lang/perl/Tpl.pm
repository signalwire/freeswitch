package Tpl;

# Copyright (c) 2005-2007, Troy Hanson      http://tpl.sourceforge.net
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;
use Config;  # to get the size of "double" on this platform

use bytes;   # always use byte (not unicode char) offsets w/tpl images

our $VERSION = 1.1;

# tpl object is a reference to a hash with these keys:
#
# A(0):  
# ... :  
# A(n):  
#
# where each A(i) refers to an A node, except A(0) is the root node.
#
# For each hash key (A node or root node), the value of that key is
# a list reference. The members are of the list are the node's children.
# They're represented as "Ai" (for A nodes) where i is a positive integer;
# for non-A nodes the representation is [type,addr] e.g. [ "i", \$some_integer]
# 
# For example, 
# Tpl->map("iA(ib)", \$x, \$y, \$z);
# returns a tpl object which is a reference to a hash with these keys/values:
#
# $self->{A0} = [ [ "i", \$x ], "A1" ];
# $self->{A1} = [ [ "i", \$y ], [ "b", \$z ] ];
#
# Now if A1 (that is, the "A(ib)" node) is packed, the tpl object acquires
# another hash key/value:
# $self->{P1} = [ $binary_int, $binary_byte ];  
# and repeated calls to pack A1 append further $binary elements.
#
sub tpl_map {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $fmt = shift;
    my @astack = (0); # stack of current A node's lineage in tpl tree
    my $a_count=0;    # running count of A's, thus an index of them
    my $self = {};    # populate below
    my ($lparen_level,$expect_lparen,$in_structure)=(0,0,0);
    for (my $i=0; $i < length $fmt; $i++) {
        my $c = substr($fmt,$i,1);
        if ($c eq 'A') {
            $a_count++;
            push @{ $self->{"A" . $astack[-1]} }, "A$a_count";
            push @astack, $a_count;
            $expect_lparen=1;
        } elsif ($c eq '(') {
            die "invalid format $fmt" unless $expect_lparen;
            $expect_lparen=0;
            $lparen_level++;
        } elsif ($c eq ')') {
            $lparen_level--;
            die "invalid format $fmt" if $lparen_level < 0;
            die "invalid format $fmt" if substr($fmt,$i-1,1) eq '(';
            if ($in_structure && ($in_structure-1 == $lparen_level)) {
                $in_structure=0; 
            } else { 
                pop @astack;  # rparen ends A() type, not S() type 
            }
        } elsif ($c eq 'S') {
            # in perl we just parse and ignore the S() construct
            $expect_lparen=1;
            $in_structure=1+$lparen_level; # so we can tell where S fmt ends 
        } elsif ($c =~ /^(i|u|B|s|c|f|I|U)$/) {
            die "invalid format $fmt" if $expect_lparen;
            my $r = shift;
            die "no reference for $c (position $i of $fmt)" unless ref($r);
            if (($c eq "f") and ($Config{doublesize} != 8)) {
               die "double not 8 bytes on this platform";
            }
            if (($c =~ /(U|I)/) and not defined ($Config{use64bitint})) {
               die "Tpl.pm: this 32-bit Perl can't pack/unpack 64-bit I/U integers\n";
            }
            push @{ $self->{"A" . $astack[-1]} }, [ $c , $r ];
        } elsif ($c eq "#") {
            #  test for previous iucfIU
            die "unallowed length modifer" unless $self->{"A" . $astack[-1]}->[-1]->[0] =~ /^(i|u|c|I|U|f)$/;
            my $n = shift;
            die "non-numeric # length modifer" unless $n =~ /^\d+$/;
            push @{ $self->{"A" . $astack[-1]}->[-1] }, $n;
            push @{ $self->{"#"}}, $n;  # master array of octothorpe lengths
        } else {
            die "invalid character $c in format $fmt";
        }
    }
    die "invalid format $fmt" if $lparen_level != 0;
    $self->{fmt} = $fmt;
    bless $self;
    return $self;
}

sub tpl_format {
    my $self = shift;
    return $self->{fmt};
}

sub tpl_pack {
    my $self = shift;
    my $i = shift;
    die "invalid index" unless defined $self->{"A$i"};
    die "tpl for unpacking only" if defined $self->{"loaded"};
    $self->{"packed"}++;
    $self->{"P$i"} = undef if $i == 0;  # node 0 doesn't accumulate
    my @bb;
    foreach my $node (@{ $self->{"A$i"} }) {
        if (ref($node)) {
            my ($type,$addr,$fxlen) = @{ $node };
            if (defined $fxlen) { # octothorpic array 
                push @bb, CORE::pack("l$fxlen",@$addr) if $type eq "i"; # int
                push @bb, CORE::pack("L$fxlen",@$addr) if $type eq "u"; # uint
                push @bb, CORE::pack("C$fxlen",@$addr) if $type eq "c"; # byte
                push @bb, CORE::pack("d$fxlen",@$addr) if $type eq "f"; # double
                push @bb, CORE::pack("q$fxlen",@$addr) if $type eq "I"; # int64
                push @bb, CORE::pack("Q$fxlen",@$addr) if $type eq "U"; # uint64
            } else {
                # non-octothorpic singleton
                push @bb, CORE::pack("l",$$addr) if $type eq "i"; # int
                push @bb, CORE::pack("L",$$addr) if $type eq "u"; # uint
                push @bb, CORE::pack("C",$$addr) if $type eq "c"; # byte
                push @bb, CORE::pack("d",$$addr) if $type eq "f"; # double (8 byte)
                push @bb, CORE::pack("q",$$addr) if $type eq "I"; # int64
                push @bb, CORE::pack("Q",$$addr) if $type eq "U"; # uint64
                if ($type =~ /^(B|s)$/) {                         # string/binary
                    push @bb, CORE::pack("L", length($$addr));
                    push @bb, CORE::pack("a*", $$addr);
                }
            }
        } elsif ($node =~ /^A(\d+)$/) {
            # encode array length (int) and the array data into one scalar
            my $alen = pack("l", scalar @{ $self->{"P$1"} or [] });
            my $abod = (join "", @{ $self->{"P$1"} or [] });
            push @bb, $alen . $abod;
            $self->{"P$1"} = undef;
        } else {
          die "internal error; invalid node symbol $node";
        }
    }
    push @{ $self->{"P$i"} }, (join "", @bb);
}

sub big_endian {
    return (CORE::unpack("C", CORE::pack("L",1)) == 1) ? 0 : 1;
}

sub tpl_dump {
    my $self = shift;
    my $filename = shift;

    $self->tpl_pack(0) if not defined $self->{"P0"};  
    my $format = $self->tpl_format;
    my $octothorpe_lens = CORE::pack("L*", @{ $self->{"#"} or [] });
    my $data = (join "", @{ $self->{"P0"} });  
    my $ov_len = length($format) + 1 + length($octothorpe_lens) + length($data) + 8;
    my $flags = big_endian() ? 1 : 0;
    my $preamble = CORE::pack("CLZ*", $flags, $ov_len, $format);
    my $tpl = "tpl" . $preamble . $octothorpe_lens . $data;
    return $tpl unless $filename;

    # here for file output
    open TPL, ">$filename" or die "can't open $filename: $!";
    print TPL $tpl;
    close TPL;
}

sub tpl_peek {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $tplhandle = shift;
    my $tpl;

    if (ref($tplhandle)) {
        $tpl = $$tplhandle;
    } else {
        open TPL, "<$tplhandle" or die "can't open $tplhandle: $!";
        undef $/;   # slurp
        $tpl = <TPL>;
        close TPL;
    }
    die "invalid tpl file" unless ($tpl =~ /^tpl/);
    return (unpack("Z*", substr($tpl,8)));
}

sub tpl_load {
    my $self = shift;
    my $tplhandle = shift;

    die "tpl for packing only" if $self->{"packed"};
    die "tpl reloading not supported" if $self->{"loaded"};

    # read tpl image from file or was it passed directly via ref?
    my $tpl;
    if (ref($tplhandle)) {
        $tpl = $$tplhandle;
    } else {
        open TPL, "<$tplhandle" or die "can't open $tplhandle: $!";
        undef $/;   # slurp
        $tpl = <TPL>;
        close TPL;
    }
    
    $self->{"TI"} = $tpl;
    $self->{"TL"} = length $tpl;
    # verify preamble
    die "invalid image -1" unless length($tpl) >= 9;
    die "invalid image -2" unless $tpl =~ /^tpl/;
    my $flags = CORE::unpack("C", substr($tpl,3,1));
    $self->{"xendian"} = 1 if (big_endian() != ($flags & 1));
    $self->{"UF"} = ($flags & 1) ? "N" : "V";
    my $ov_len = CORE::unpack($self->{"UF"}, substr($tpl,4,4));
    die "invalid image -3" unless $ov_len == length($tpl);
    my $format = CORE::unpack("Z*", substr($tpl,8));
    die "format mismatch" unless $format eq $self->tpl_format();
    my @octothorpe_lens = @{ $self->{"#"} or [] };
    my $ol = 8 + length($format) + 1; # start of octothorpe lengths
    for (my $i=0; $i < (scalar @octothorpe_lens); $i++) {
        my $len = CORE::unpack($self->{"UF"}, substr($tpl,$ol,4));
        my $olen = $octothorpe_lens[$i];
        die "fixed-length array size mismatch" unless $olen == $len;
        $ol += 4;
    }
    my $dv = $ol;  # start of packed data 
    my $len = $self->serlen("A0",$dv);
    die "invalid image -4" if $len == -1;
    die "invalid image -5" if (length($tpl) != $len + $dv);
    $self->{"C0"} = $dv;
    $self->{"loaded"} = 1;
    $self->unpackA0;   # prepare root child nodes for use
}

# byte reverse a word (any length)
sub reversi {
    my $word = shift;
    my @w = split //, $word;
    my $r = join "", (reverse @w);
    return $r;
}

#
# while unpacking, the object has these keys in its hash:
# C0
# C1
# ...
# C<n>
# These are indices (into the tpl image $self->{"TI"}) from which node n
# is being unpacked. I.e. as array elements of node n are unpacked, C<n>
# advances through the tpl image.
# 
# Similarly, elements
# N1
# N2
# ...
# N<n>
# refer to the remaining array count for node n.
#
sub tpl_unpack {
    my $self = shift;
    my $n = shift;
    my $ax = "A$n";
    my $cx = "C$n";
    my $nx = "N$n";
    my $rc;

    die "tpl for packing only" if $self->{"packed"};
    die "tpl not loaded" unless $self->{"loaded"};

    # decrement count for non root array nodes
    if ($n > 0) {
        return 0 if $self->{$nx} <= 0;
        $rc = $self->{$nx}--;
    }

    for my $c (@{ $self->{$ax} }) {
        if (ref($c)) {
            my ($type,$addr,$fxlen) = @$c;
            if (defined $fxlen) {  # octothorpic unpack
                @{ $addr } = (); # empty existing list before pushing elements
                for(my $i=0; $i < $fxlen; $i++) {
                    if ($type eq "u") {  # uint
                        push @{ $addr }, CORE::unpack($self->{"UF"},
                                     substr($self->{"TI"},$self->{$cx},4));
                        $self->{$cx} += 4;
                    } elsif ($type eq "i") { #int (see note below re:signed int)
                        my $intbytes = substr($self->{"TI"},$self->{$cx},4);
                        $intbytes = reversi($intbytes) if $self->{"xendian"};
                        push @{ $addr }, CORE::unpack("l", $intbytes);
                        $self->{$cx} += 4;
                    } elsif ($type eq "c") { # byte
                        push @{ $addr }, CORE::unpack("C",
                                     substr($self->{"TI"},$self->{$cx},1));
                        $self->{$cx} += 1;
                    } elsif ($type eq "f") { # double
                        my $double_bytes = substr($self->{"TI"},$self->{$cx},8);
                        $double_bytes = reversi($double_bytes) if $self->{"xendian"};
                        push @{ $addr }, CORE::unpack("d", $double_bytes );
                        $self->{$cx} += 8;
                    } elsif ($type eq "I") { #int64 
                        my $intbytes = substr($self->{"TI"},$self->{$cx},8);
                        $intbytes = reversi($intbytes) if $self->{"xendian"};
                        push @{ $addr }, CORE::unpack("q", $intbytes);
                        $self->{$cx} += 8;
                    } elsif ($type eq "U") { #uint64 
                        my $intbytes = substr($self->{"TI"},$self->{$cx},8);
                        $intbytes = reversi($intbytes) if $self->{"xendian"};
                        push @{ $addr }, CORE::unpack("Q", $intbytes);
                        $self->{$cx} += 8;
                    }
                }
            } else {
                # non-octothorpe (singleton)
                if ($type eq "u") {       # uint
                    ${$addr} = CORE::unpack($self->{"UF"},
                                 substr($self->{"TI"},$self->{$cx},4));
                    $self->{$cx} += 4;
                } elsif ($type eq "i") {       # int
                    # while perl's N or V conversions unpack an unsigned
                    # long from either big or little endian format 
                    # respectively, when it comes to *signed* int, perl
                    # only has 'l' (which assumes native endianness). 
                    # So we have to manually reverse the bytes in a 
                    # cross-endian 'int' unpacking scenario.
                    my $intbytes = substr($self->{"TI"},$self->{$cx},4);
                    $intbytes = reversi($intbytes) if $self->{"xendian"};
                    ${$addr} = CORE::unpack("l", $intbytes);
                    $self->{$cx} += 4;
                } elsif ($type eq 'c') {  # byte
                    ${$c->[1]} = CORE::unpack("C",
                                 substr($self->{"TI"},$self->{$cx},1));
                    $self->{$cx} += 1;
                } elsif ($type eq 'f') {  # double
                    ${$addr} = CORE::unpack("d",
                                 substr($self->{"TI"},$self->{$cx},8));
                    $self->{$cx} += 8;
                } elsif ($type =~ /^(B|s)$/) {  # string/binary
                    my $slen = CORE::unpack($self->{"UF"},
                                 substr($self->{"TI"},$self->{$cx},4));
                    $self->{$cx} += 4;
                    ${$addr} = CORE::unpack("a$slen",
                                 substr($self->{"TI"},$self->{$cx},$slen));
                    $self->{$cx} += $slen;
                } elsif ($type eq "I") {       # int64
                    my $intbytes = substr($self->{"TI"},$self->{$cx},8);
                    $intbytes = reversi($intbytes) if $self->{"xendian"};
                    ${$addr} = CORE::unpack("q", $intbytes);
                    $self->{$cx} += 8;
                } elsif ($type eq "U") {       # uint64
                    my $intbytes = substr($self->{"TI"},$self->{$cx},8);
                    $intbytes = reversi($intbytes) if $self->{"xendian"};
                    ${$addr} = CORE::unpack("Q", $intbytes);
                    $self->{$cx} += 8;
                } else { die "internal error"; }
            }
        } elsif ($c =~ /^A(\d+)$/) {
            my $alen = $self->serlen($c,$self->{$cx});
            $self->{"N$1"} = CORE::unpack($self->{"UF"},
                     substr($self->{"TI"},$self->{$cx},4)); # get array count
            $self->{"C$1"} = $self->{$cx} + 4;  # set array node's data start
            $self->{$cx} += $alen;              # step over array node's data
        } else { die "internal error"; }
    }

    return $rc;
}

# specialized function to prepare root's child A nodes for initial use
sub unpackA0 {
    my $self = shift;
    my $ax = "A0";
    my $cx = "C0";
    my $c0 = $self->{$cx};

    for my $c (@{ $self->{$ax} }) {
        next if ref($c); # skip non-A nodes
        if ($c =~ /^A(\d+)$/) {
            my $alen = $self->serlen($c,$c0);
            $self->{"N$1"} = CORE::unpack($self->{"UF"},
                     substr($self->{"TI"},$c0,4)); # get array count
            $self->{"C$1"} = $c0 + 4;  # set array node's data start
            $c0 += $alen;              # step over array node's data
        } else { die "internal error"; }
    }
}

# ascertain serialized length of given node by walking
sub serlen {
    my $self = shift;
    my $ax = shift;
    my $dv = shift;

    my $len = 0;

    my $num;  
    if ($ax eq "A0") {
        $num = 1;
    } else {
        return -1 unless $self->{"TL"} >= $dv + 4;
        $num = CORE::unpack($self->{"UF"},substr($self->{"TI"},$dv,4));
        $dv += 4;
        $len += 4;
    }

    while ($num-- > 0) {
        for my $c (@{ $self->{$ax} }) {
            if (ref($c)) {
                my $n = 1;
                $n = $c->[2] if (@$c > 2); # octothorpic array length
                if ($c->[0] =~ /^(i|u)$/) {       # int/uint
                    return -1 unless $self->{"TL"} >= $dv + 4*$n;
                    $len += 4*$n;
                    $dv += 4*$n;
                } elsif ($c->[0] eq "c") {  # byte
                    return -1 unless $self->{"TL"} >= $dv + 1*$n;
                    $len += 1*$n;
                    $dv += 1*$n;
                } elsif ($c->[0] eq "f") {  # double
                    return -1 unless $self->{"TL"} >= $dv + 8*$n;
                    $len += 8*$n;
                    $dv += 8*$n;
                } elsif ($c->[0] =~ /(I|U)/) {  # int64/uint64
                    return -1 unless $self->{"TL"} >= $dv + 8*$n;
                    $len += 8*$n;
                    $dv += 8*$n;
                } elsif ($c->[0] =~ /^(B|s)$/) {  # string/binary
                    return -1 unless $self->{"TL"} >= $dv + 4;
                    my $slen = CORE::unpack($self->{"UF"},
                                substr($self->{"TI"},$dv,4));
                    $len += 4;
                    $dv += 4;
                    return -1 unless $self->{"TL"} >= $dv + $slen;
                    $len += $slen;
                    $dv += $slen;
                } else { die "internal error" }
            } elsif ($c =~ /^A/) {
                my $alen = $self->serlen($c,$dv);
                return -1 if $alen == -1;
                $dv += $alen;
                $len += $alen;
            } else { die "internal error"; }
        }
    }
    return $len;
}

1
