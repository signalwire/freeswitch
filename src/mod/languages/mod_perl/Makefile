PERL        =/usr/local/bin/perl
PERL_LIBDIR =-L$(shell perl -MConfig -e 'print $$Config{archlib}')/CORE
PERL_LIBS   =$(shell perl -MConfig -e 'print $$Config{libs}')
CFLAGS  +=  -DMULTIPLICITY $(shell $(PERL) -MExtUtils::Embed -e ccopts)
CFLAGS  += -DEMBED_PERL
LDFLAGS +=  $(shell $(PERL) -MExtUtils::Embed -e ldopts)
LDFLAGS +=  $(shell $(PERL) -MConfig -e 'print $$Config{libs}')  
OBJS += perlxsi.o



all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) fs_perl.$(DYNAMIC_LIB_EXTEN)

.perlok:
	@(${PERL} -V | grep -i usemultiplicity=define >/dev/null && echo Phew, You have the right perl.) \
	|| ((echo Sorry, you need to compile perl with threads and multiplicity.&& exit 1))
	@touch .perlok

perlxsi.c: .perlok
	perl -MExtUtils::Embed -e xsinit

depends: perlxsi.c

%.o:  %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

reswig: 
	rm switch_swig_wrap.c
	swig -DMULTIPLICITY -perl5 -module fs_perl switch_swig.c

switch_swig_wrap.o: switch_swig_wrap.c
	$(CC) -w $(CFLAGS) -fPIC -c $< -o $@


fs_perl.$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o
	$(CC) $(SOLINK) -o fs_perl.$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS)
	$(CC) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .perlok

install:
	mkdir -p $(PREFIX)/perl
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
	cp -f fs_perl.$(DYNAMIC_LIB_EXTEN) fs_perl.pm $(PREFIX)/perl
	if [ ! -f $(PREFIX)/perl/freeswitch.pm ] ; then cp -f freeswitch.pm $(PREFIX)/perl ; fi
