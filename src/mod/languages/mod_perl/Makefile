# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

PERL        = `which perl`
PERL_LIBDIR =-L$(shell perl -MConfig -e 'print $$Config{archlib}')/CORE
PERL_LIBS   =$(shell perl -MConfig -e 'print $$Config{libs}')

LOCAL_CFLAGS=-w -DMULTIPLICITY $(shell $(PERL) -MExtUtils::Embed -e ccopts) -DEMBED_PERL
LOCAL_LDFLAGS=$(shell $(PERL) -MExtUtils::Embed -e ldopts) $(shell $(PERL) -MConfig -e 'print $$Config{libs}')
LOCAL_OBJS=perlxsi.o
include ../../../../build/modmake.rules


SWIGCFILE=../../../switch_swig.c
SWIGIFILE=../../../switch_swig.i



local_all: fs_perl.$(DYNAMIC_LIB_EXTEN)

.perlok:
	@(${PERL} -V | grep -i usemultiplicity=define >/dev/null && echo Phew, You have the right perl.) \
	|| ((echo Sorry, you need to compile perl with threads and multiplicity.&& exit 1))
	@touch .perlok

reswig: 
	rm switch_swig_wrap.c
	swig -o switch_swig_wrap.c -l$(SWIGIFILE) -ignoremissing -DMULTIPLICITY -perl5 -module fs_perl $(SWIGCFILE)

switch_swig.o: $(SWIGCFILE) Makefile
	$(CC)  -w $(ALL_CFLAGS) -c $< -o $@

switch_swig_wrap.o: switch_swig_wrap.c
	$(CC) -w $(ALL_CFLAGS) -c $< -o $@

fs_perl.$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o
	$(LINK) $(SOLINK) -o fs_perl.$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)

local_clean:
	rm -fr *~ .perlok

depend_install:
	mkdir -p $(PREFIX)/perl
	$(LTINSTALL) fs_perl.$(DYNAMIC_LIB_EXTEN) fs_perl.pm $(PREFIX)/perl
	if [ ! -f $(PREFIX)/perl/freeswitch.pm ] ; then $(LTINSTALL) freeswitch.pm $(PREFIX)/perl ; fi
