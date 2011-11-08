#!/usr/bin/make -f

# The auto tools MUST be run in the following order:
#
#	1.  aclocal
#	2.  libtoolize (if you use libtool)
#	3.  autoconf
#	4.  autoheader (if you use autoheader)
#	5.  automake (if you use automake)
#
# The following makefile runs these in the correct order according to their
# dependancies. It also makes up for Mac OSX's fucked-upped-ness.

ACLOCAL = aclocal

ifneq ($(shell uname -s), Darwin)
  LIBTOOLIZE = libtoolize
else
  # Fuck Apple! Why the hell did they rename libtoolize????
  LIBTOOLIZE = glibtoolize
  # Fink sucks as well, but this seems necessary.
  ACLOCAL_INC = -I /sw/share/aclocal
endif

genfiles : config.status
	(cd src && make genfiles)
	(cd tests && make genfiles)

config.status: configure src/config.h.in Makefile.in src/Makefile.in tests/Makefile.in
	./configure --enable-gcc-werror

configure: ltmain.sh
	autoconf

Makefile.in: Makefile.am	
	automake --copy --add-missing

src/Makefile.in: src/Makefile.am	
	automake --copy --add-missing

tests/Makefile.in: tests/Makefile.am	
	automake --copy --add-missing

src/config.h.in: configure
	autoheader

libtool ltmain.sh: aclocal.m4
	$(LIBTOOLIZE) --copy --force
	
# Need to re-run aclocal whenever acinclude.m4 is modified.
aclocal.m4: acinclude.m4
	$(ACLOCAL) $(ACLOCAL_INC)

clean:
	rm -f libtool ltmain.sh aclocal.m4 Makefile.in src/config.h.in config.cache config.status
	find . -name .deps -type d -exec rm -rf {} \;


# Do not edit or modify anything in this comment block.
# The arch-tag line is a file identity tag for the GNU Arch 
# revision control system.
#
# arch-tag: 2b02bfd0-d5ed-489b-a554-2bf36903cca9
