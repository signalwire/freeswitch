##
##  OSSP uuid - Universally Unique Identifier
##  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
##  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
##  Copyright (c) 2004 Piotr Roszatycki <dexter@debian.org>
##
##  This file is part of OSSP uuid, a library for the generation
##  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
##
##  Permission to use, copy, modify, and distribute this software for
##  any purpose with or without fee is hereby granted, provided that
##  the above copyright notice and this permission notice appear in all
##  copies.
##
##  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
##  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
##  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
##  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
##  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
##  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
##  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
##  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
##  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
##  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
##  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
##  SUCH DAMAGE.
##
##  uuid_compat.pm: Data::UUID Backward Compatibility Perl API
##

package Data::UUID;

use 5.006;
use warnings;
use strict;

use OSSP::uuid;
use MIME::Base64 qw();

require Exporter;

our @ISA     = qw(Exporter);
our @EXPORT  = qw(NameSpace_DNS NameSpace_OID NameSpace_URL NameSpace_X500);

our $VERSION = do { my @v = ('1.6.2' =~ m/\d+/g); sprintf("%d.".("%02d"x$#v), @v); };

sub new {
    my $class = shift;
    my $self = bless {}, $class;
    return $self;
}

sub create {
    my ($self) = @_;
    my $uuid = OSSP::uuid->new;
    $uuid->make('v4');
    return $uuid->export('bin');
}

sub create_from_name {
    my ($self, $nsid, $name) = @_;
    my $uuid = OSSP::uuid->new;
    my $nsiduuid = OSSP::uuid->new;
    $nsiduuid->import('bin', $nsiduuid);
    $uuid = OSSP::uuid->new;
    $uuid->make('v3', $nsiduuid, $name);
    return $uuid->export('bin');
}

sub to_string {
    my ($self, $bin) = @_;
    my $uuid = OSSP::uuid->new;
    $uuid->import('bin', $bin);
    return $uuid->export('str');
}

sub to_hexstring {
    my ($self, $bin) = @_;
    my $uuid = OSSP::uuid->new;
    $uuid->import('bin', $bin);
    (my $string = '0x' . $uuid->export('str')) =~ s/-//g;
    return $string;
}

sub to_b64string {
    my ($self, $bin) = @_;
    return MIME::Base64::encode_base64($bin, '');
}

sub from_string {
    my ($self, $str) = @_;
    my $uuid = OSSP::uuid->new;
    $uuid->import('str',
          $str =~ /^0x/
        ? join '-', unpack('x2 a8 a4 a4 a4 a12', $str)
        : $str
    );
    return $uuid->export('bin');
}

sub from_hexstring {
    my ($self, $str) = @_;
    my $uuid = OSSP::uuid->new;
    $uuid->import('str', join '-', unpack('x2 a8 a4 a4 a4 a12', $str));
    return $uuid->export('bin');
}

sub from_b64string {
    my ($self, $b64) = @_;
    return MIME::Base64::decode_base64($b64);
}

sub compare {
    my ($self, $bin1, $bin2) = @_;
    my $uuid1 = OSSP::uuid->new;
    my $uuid2 = OSSP::uuid->new;
    $uuid1->import('bin', $bin1);
    $uuid2->import('bin', $bin2);
    return $uuid1->compare($uuid2);
}

my %NS = (
    'NameSpace_DNS'  => 'ns:DNS',
    'NameSpace_URL'  => 'ns:URL',
    'NameSpace_OID'  => 'ns:OID',
    'NameSpace_X500' => 'ns:X500',
);

while (my ($k, $v) = each %NS) {
    no strict 'refs';
    *{$k} = sub () {
        my $uuid = OSSP::uuid->new;
        $uuid->load($v);
        return $uuid->export('bin');
    };
}

sub constant {
    my ($self, $arg) = @_;
    my $uuid = OSSP::uuid->new;
    $uuid->load($NS{$arg} || 'nil');
    return $uuid->export('bin');
}

sub create_str {
    my $self = shift;
    return $self->to_string($self->create);
}

sub create_hex {
    my $self = shift;
    return $self->to_hexstring($self->create);
}

sub create_b64 {
    my $self = shift;
    return $self->to_b64string($self->create);
}

sub create_from_name_str {
    my $self = shift;
    return $self->to_string($self->create_from_name(@_));
}

sub create_from_name_hex {
    my $self = shift;
    return $self->to_hexstring($self->create_from_name(@_));
}

sub create_from_name_b64 {
    my $self = shift;
    return $self->to_b64string($self->create_from_name(@_));
}

1;

