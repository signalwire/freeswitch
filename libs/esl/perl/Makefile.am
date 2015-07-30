if HAVE_PERL
perldir = $(PERL_SITEDIR)
perl_LTLIBRARIES = ESL.la
ESL_la_SOURCES   = esl_wrap.cpp perlxsi.c
ESL_la_CFLAGS    = $(CC_CFLAGS) $(CFLAGS) -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CFLAGS) $(PERL_CFLAGS)
ESL_la_CXXFLAGS  = -I$(switch_srcdir)/libs/esl/src/include $(SWITCH_AM_CXXFLAGS) $(CXXFLAGS) -w $(PERL_INC)
ESL_la_LDFLAGS   = -avoid-version -module -no-undefined -shared $(PERL_LDFLAGS)
ESL_la_LIBADD    = ../libesl.la

perlmod: ESL.la

install-data-local: perlmod-install

perlmod-install: install-perlLTLIBRARIES
	install -d -m 755 ESL $(DESTDIR)$(PERL_SITEDIR)/ESL
	install -m 755 ESL.pm $(DESTDIR)$(PERL_SITEDIR)
	install -m 755 ESL/* $(DESTDIR)$(PERL_SITEDIR)/ESL
endif

../libesl.la:
	$(MAKE) -C ..

esl_wrap.cpp:
	swig2.0 -module ESL -shadow -perl5 -c++ -DMULTIPLICITY -I../src/include -o esl_wrap.cpp ../ESL.i

perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

clean:
	rm -f *.o *.so *~

swigclean:
	rm -f esl_wrap.* ESL.so ESL.pm perlxsi.*

reswig:	swigclean esl_wrap.cpp perlxsi.c

