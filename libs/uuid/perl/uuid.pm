##
##  OSSP uuid - Universally Unique Identifier
##  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
##  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
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
##  uuid.pm: Perl Binding (Perl part)
##

##
##  High-Level Perl Module TIE-style API
##  (just a functionality-reduced TIE wrapper around the OO-style API)
##

package OSSP::uuid::tie;

use 5.008;
use strict;
use warnings;
use Carp;

#   inhert from Tie::Scalar
require Tie::Scalar;
our @ISA = qw(Tie::Scalar);

#   helper function
sub mode_sanity {
    my ($mode) = @_;
    if (not (    defined($mode)
             and ref($mode) eq 'ARRAY'
             and (   (@{$mode} == 1 and $mode->[0] =~ m|^v[14]$|)
                  or (@{$mode} == 3 and $mode->[0] =~ m|^v[35]$|)))) {
        return (undef, "invalid UUID generation mode specification");
    }
    if ($mode->[0] =~ m|^v[35]$|) {
        my $uuid_ns = new OSSP::uuid;
        $uuid_ns->load($mode->[1])
            or return (undef, "failed to load UUID $mode->[0] namespace");
        $mode->[1] = $uuid_ns;
    }
    return ($mode, undef);
}

#   constructor
sub TIESCALAR {
    my ($class, @args) = @_;
    my $self = {};
    bless ($self, $class);
    $self->{-uuid} = new OSSP::uuid
       or croak "failed to create OSSP::uuid object";
    my ($mode, $error) = mode_sanity(defined($args[0]) ? [ @args ] : [ "v1" ]);
    croak $error if defined($error);
    $self->{-mode} = $mode;
    return $self;
}

#   destructor
sub DESTROY {
    my ($self) = @_;
    delete $self->{-uuid};
    delete $self->{-mode};
    return;
}

#   fetch value from scalar
#   (applied semantic: export UUID in string format)
sub FETCH {
    my ($self) = @_;
    $self->{-uuid}->make(@{$self->{-mode}})
       or croak "failed to generate new UUID";
    my $value = $self->{-uuid}->export("str")
       or croak "failed to export new UUID";
    return $value;
}

#   store value into scalar
#   (applied semantic: configure new UUID generation mode)
sub STORE {
    my ($self, $value) = @_;
    my ($mode, $error) = mode_sanity($value);
    croak $error if defined($error);
    $self->{-mode} = $mode;
    return;
}

##
##  High-Level Perl Module OO-style API
##  (just an OO wrapper around the C-style API)
##

package OSSP::uuid;

use 5.008;
use strict;
use warnings;
use Carp;
use XSLoader;
use Exporter;

#   API version
our $VERSION = do { my @v = ('1.6.2' =~ m/\d+/g); sprintf("%d.".("%02d"x$#v), @v); };

#   API inheritance
our @ISA = qw(Exporter);

#   API symbols
my $symbols = {
    'const' => [qw(
        UUID_VERSION
        UUID_LEN_BIN
        UUID_LEN_STR
        UUID_LEN_SIV
        UUID_RC_OK
        UUID_RC_ARG
        UUID_RC_MEM
        UUID_RC_SYS
        UUID_RC_INT
        UUID_RC_IMP
        UUID_MAKE_V1
        UUID_MAKE_V3
        UUID_MAKE_V4
        UUID_MAKE_V5
        UUID_MAKE_MC
        UUID_FMT_BIN
        UUID_FMT_STR
        UUID_FMT_SIV
        UUID_FMT_TXT
    )],
    'func' => [qw(
        uuid_create
        uuid_destroy
        uuid_load
        uuid_make
        uuid_isnil
        uuid_compare
        uuid_import
        uuid_export
        uuid_error
        uuid_version
    )]
};

#   API symbol exportation
our %EXPORT_TAGS = (
    'all'   => [ @{$symbols->{'const'}}, @{$symbols->{'func'}} ],
    'const' => [ @{$symbols->{'const'}} ],
    'func'  => [ @{$symbols->{'func'}}  ]
);
our @EXPORT_OK = @{$EXPORT_TAGS{'all'}};
our @EXPORT    = ();

#   constructor
sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self = {};
    bless ($self, $class);
    $self->{-uuid} = undef;
    $self->{-rc}   = $self->UUID_RC_OK;
    my $rc = uuid_create($self->{-uuid});
    if ($rc != $self->UUID_RC_OK) {
        croak(sprintf("OSSP::uuid::new: uuid_create: %s (%d)", uuid_error($rc), $rc));
        return undef;
    }
    return $self;
}

#   destructor
sub DESTROY ($) {
    my ($self) = @_;
    $self->{-rc} = uuid_destroy($self->{-uuid}) if (defined($self->{-uuid}));
    if ($self->{-rc} != $self->UUID_RC_OK) {
        carp(sprintf("OSSP::uuid::DESTROY: uuid_destroy: %s (%d)", uuid_error($self->{-rc}), $self->{-rc}));
        return;
    }
    $self->{-uuid} = undef;
    $self->{-rc}   = undef;
    return;
}

sub load ($$) {
    my ($self, $name) = @_;
    $self->{-rc} = uuid_load($self->{-uuid}, $name);
    return ($self->{-rc} == $self->UUID_RC_OK);
}

sub make ($$;@) {
    my ($self, $mode, @valist) = @_;
    my $mode_code = 0;
    foreach my $spec (split(/,/, $mode)) {
        if    ($spec eq 'v1') { $mode_code |= $self->UUID_MAKE_V1; }
        elsif ($spec eq 'v3') { $mode_code |= $self->UUID_MAKE_V3; }
        elsif ($spec eq 'v4') { $mode_code |= $self->UUID_MAKE_V4; }
        elsif ($spec eq 'v5') { $mode_code |= $self->UUID_MAKE_V5; }
        elsif ($spec eq 'mc') { $mode_code |= $self->UUID_MAKE_MC; }
        else  { croak("invalid mode specification \"$spec\""); }
    }
    if (($mode_code & $self->UUID_MAKE_V3) or ($mode_code & $self->UUID_MAKE_V5)) {
        if (not (ref($valist[0]) and $valist[0]->isa("OSSP::uuid"))) {
            croak("UUID_MAKE_V3/UUID_MAKE_V5 requires namespace argument to be OSSP::uuid object");
        }
        my $ns   = $valist[0]->{-uuid};
        my $name = $valist[1];
        $self->{-rc} = uuid_make($self->{-uuid}, $mode_code, $ns, $name);
    }
    else {
        $self->{-rc} = uuid_make($self->{-uuid}, $mode_code);
    }
    return ($self->{-rc} == $self->UUID_RC_OK);
}

sub isnil ($) {
    my ($self) = @_;
    my $result;
    $self->{-rc} = uuid_isnil($self->{-uuid}, $result);
    return ($self->{-rc} == $self->UUID_RC_OK ? $result : undef);
}

sub compare ($$) {
    my ($self, $other) = @_;
    my $result = 0;
    if (not (ref($other) and $other->isa("OSSP::uuid"))) {
        croak("argument has to an OSSP::uuid object");
    }
    $self->{-rc} = uuid_compare($self->{-uuid}, $other->{-uuid}, $result);
    return ($self->{-rc} == $self->UUID_RC_OK ? $result : undef);
}

sub import {
    #   ATTENTION: The OSSP uuid API function "import" conflicts with
    #   the standardized "import" method the Perl world expects from
    #   their modules. In order to keep the Perl binding consist
    #   with the C API, we solve the conflict under run-time by
    #   distinguishing between the two types of "import" calls.
    if (defined($_[0]) and ref($_[0]) =~ m/^OSSP::uuid/) {
        #   the regular OSSP::uuid "import" method
        croak("import method expects 3 or 4 arguments") if (@_ < 3 or @_ > 4); # emulate prototype
        my ($self, $fmt, $data_ptr, $data_len) = @_;
        if    ($fmt eq 'bin') { $fmt = $self->UUID_FMT_BIN; }
        elsif ($fmt eq 'str') { $fmt = $self->UUID_FMT_STR; }
        elsif ($fmt eq 'siv') { $fmt = $self->UUID_FMT_SIV; }
        elsif ($fmt eq 'txt') { $fmt = $self->UUID_FMT_TXT; }
        else  { croak("invalid format \"$fmt\""); }
        $data_len ||= length($data_ptr); # functional redudant, but Perl dislikes undef value here
        $self->{-rc} = uuid_import($self->{-uuid}, $fmt, $data_ptr, $data_len);
        return ($self->{-rc} == $self->UUID_RC_OK);
    }
    else {
        #   the special Perl "import" method
        #   (usually inherited from the Exporter)
        no strict "refs";
        return OSSP::uuid->export_to_level(1, @_);
    }
}

sub export {
    #   ATTENTION: The OSSP uuid API function "export" conflicts with
    #   the standardized "export" method the Perl world expects from
    #   their modules. In order to keep the Perl binding consist
    #   with the C API, we solve the conflict under run-time by
    #   distinguishing between the two types of "export" calls.
    if (defined($_[0]) and ref($_[0]) =~ m/^OSSP::uuid/) {
        #   the regular OSSP::uuid "export" method
        croak("export method expects 2 arguments") if (@_ != 2); # emulate prototype
        my ($self, $fmt) = @_;
        my $data_ptr;
        if    ($fmt eq 'bin') { $fmt = $self->UUID_FMT_BIN; }
        elsif ($fmt eq 'str') { $fmt = $self->UUID_FMT_STR; }
        elsif ($fmt eq 'siv') { $fmt = $self->UUID_FMT_SIV; }
        elsif ($fmt eq 'txt') { $fmt = $self->UUID_FMT_TXT; }
        else  { croak("invalid format \"$fmt\""); }
        $self->{-rc} = uuid_export($self->{-uuid}, $fmt, $data_ptr, undef);
        return ($self->{-rc} == $self->UUID_RC_OK ? $data_ptr : undef);
    }
    else {
        #   the special Perl "export" method
        #   (usually inherited from the Exporter)
        return Exporter::export(@_);
    }
}

sub error ($;$) {
    my ($self, $rc) = @_;
    $rc = $self->{-rc} if (not defined($rc));
    return wantarray ? (uuid_error($rc), $rc) : uuid_error($rc);
}

sub version (;$) {
    my ($self) = @_;
    return uuid_version();
}

##
##  Low-Level Perl XS C-style API
##  (actually just the activation of the XS part)
##

#   auto-loading constants
sub AUTOLOAD {
    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&OSSP::uuid::constant not defined" if ($constname eq 'constant');
    my ($error, $val) = constant($constname);
    croak $error if ($error);
    { no strict 'refs'; *$AUTOLOAD = sub { $val }; }
    goto &$AUTOLOAD;
}

#   static-loading functions
XSLoader::load('OSSP::uuid', $VERSION);

1;

