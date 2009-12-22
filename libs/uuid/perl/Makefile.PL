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
##  Makefile.PL: Perl MakeMaker build source procedure
##

require 5.008;

use Config;
use ExtUtils::MakeMaker;

#   determine source directory
my ($srcdir) = map { my $d = $_; $d =~ s/\/libuuid\.la$//; $d }
               grep { -f $_ } ("../libuuid.la", glob("../*/libuuid.la"))
    or die "no source directory found (where libuuid.la is located)";

#   determine extra build options
my $compat = 0;
@ARGV = grep { $_ =~ m/^COMPAT=(\d+)$/i ? ($compat = $1, 0) : 1 } @ARGV;

#   generate Makefile
WriteMakefile(
    NAME              => 'OSSP::uuid',
    VERSION_FROM      => 'uuid.pm',
    ABSTRACT_FROM     => 'uuid.pod',
    PREREQ_PM         => {},
    LIBS              => [ "-L$srcdir/.libs -L$srcdir -luuid" ],
    DEFINE            => '',
    INC               => "-I. -I$srcdir",
    PM                => { 'uuid.pm'   => '$(INST_LIBDIR)/uuid.pm',
                           'uuid.pod'  => '$(INST_LIBDIR)/uuid.pod',
                           ($compat ? ('uuid_compat.pm'  => '$(INST_LIBDIR)/../Data/UUID.pm')  : ()),
                           ($compat ? ('uuid_compat.pod' => '$(INST_LIBDIR)/../Data/UUID.pod') : ()), },
    MAN3PODS          => { 'uuid.pod' => '$(INST_MAN3DIR)/OSSP::uuid.3',
                           ($compat ? ('uuid_compat.pod' => '$(INST_MAN3DIR)/Data::UUID.3') : ()), },
    TYPEMAPS          => [ 'uuid.tm' ],
    test              => { TESTS => 'uuid.ts' . ($compat ? ' uuid_compat.ts' : '') },
    NO_META           => 1,
    #    cruel hack to workaround the conflict between OSSP uuid's
    #    uuid_create() function and one from FreeBSD's libc
    ((   "$Config{'osname'}$Config{'osvers'}" =~ m/^freebsd[56]\./
      and $Config{'ld'} =~ m/cc$/ and -f "/usr/include/uuid.h") ?
      ( LDDLFLAGS => $Config{'lddlflags'} . ' -Wl,-Bsymbolic') : ())
);

