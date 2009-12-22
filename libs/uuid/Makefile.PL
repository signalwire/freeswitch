##
##  OSSP uuid - Universally Unique Identifier
##  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
##  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
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
##  Makefile.PL: top-level Perl ExtUtils::MakeMaker wrapper script
##

require 5.008;
use IO::File;

my $ARGS = join(" ", @ARGV);

print "++ writing Makefile\n";

my $mk = new IO::File ">Makefile" or die;
$mk->print(<< "EOF");

PERL     = $^X
FULLPERL = $^X
ARGS     = $ARGS

all pure_all:
	\@if [ ! -d build ]; then mkdir build; fi
	\@if [ ! -f build/Makefile ]; then (cd build && ../configure --disable-shared); fi
	\@if [ ! -f build/libuuid.la ]; then (cd build && \$(MAKE) \$(MFLAGS) libuuid.la); fi
	\@if [ ! -f perl/Makefile ]; then (cd perl && \$(PERL) Makefile.PL \$(ARGS)); fi
	\@cd perl && \$(MAKE) \$(MFLAGS) \$\@

install pure_install test:
	\@cd perl && \$(MAKE) \$(MFLAGS) \$\@

clean:
	\@cd build && \$(MAKE) \$(MFLAGS) \$\@
	\@cd perl && \$(MAKE) \$(MFLAGS) \$\@

distclean realclean:
	\@cd build && \$(MAKE) \$(MFLAGS) \$\@
	\@cd perl && \$(MAKE) \$(MFLAGS) \$\@
	-rm -rf build || true
	-rm -rf Makefile || true

EOF

$mk->close();

