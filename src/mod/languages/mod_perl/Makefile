BASE=../../../..
PERL        = `which perl`
PERL_LIBDIR =-L$(shell perl -MConfig -e 'print $$Config{archlib}')/CORE
PERL_LIBS   =$(shell perl -MConfig -e 'print $$Config{libs}')
LOCAL_CFLAGS= -w -DMULTIPLICITY $(shell $(PERL) -MExtUtils::Embed -e ccopts) -DEMBED_PERL
LOCAL_LDFLAGS=$(shell $(PERL) -MExtUtils::Embed -e ldopts) $(shell $(PERL) -MConfig -e 'print $$Config{libs}')
LOCAL_OBJS=freeswitch_perl.o mod_perl_wrap.o perlxsi.o
VERBOSE=1

include $(BASE)/build/modmake.rules


swigclean: clean
	rm -f mod_perl_wrap.* freeswitch.so freeswitch.pm

mod_perl_wrap.cpp:
	swig -static -shadow -perl5 -c++ -DMULTIPLICITY -I../../../../src/include -o mod_perl_wrap.cpp freeswitch.i
	echo "#include \"mod_perl_extra.c\"" >> mod_perl_wrap.cpp
	patch -s -p0 -i hack.diff



freeswitch.$(DYNAMIC_LIB_EXTEN): $(LOCAL_OBJS) $(LOCAL_LIBADD)
	$(LINK) $(SOLINK) -o freeswitch.$(DYNAMIC_LIB_EXTEN)  $(LOCAL_OBJS) $(LOCAL_LIBADD) $(LDFLAGS)

local_all: freeswitch.$(DYNAMIC_LIB_EXTEN)

.perlok:
	@(${PERL} -V | grep -i usemultiplicity=define >/dev/null && echo Phew, You have the right perl.) \
	|| ((echo Sorry, you need to compile perl with threads and multiplicity.&& exit 1))
	@touch .perlok

local_clean:
	rm -fr *~ .perlok freeswitch.$(DYNAMIC_LIB_EXTEN)

depend_install:
	mkdir -p $(DESTDIR)$(PREFIX)/perl
	$(LTINSTALL) freeswitch.$(DYNAMIC_LIB_EXTEN) freeswitch.pm $(DESTDIR)$(PREFIX)/perl
	if [ ! -f $(DESTDIR)$(PREFIX)/perl/freeswitch.pm ] ; then $(LTINSTALL) freeswitch.pm $(DESTDIR)$(PREFIX)/perl ; fi
