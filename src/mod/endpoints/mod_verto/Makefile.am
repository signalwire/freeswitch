include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_verto

mod_LTLIBRARIES       = mod_verto.la
mod_verto_la_SOURCES  = mod_verto.c ws.c mcast/mcast.c
mod_verto_la_CFLAGS   = -D__EXTENSIONS__ -D_GNU_SOURCE $(AM_CFLAGS)
mod_verto_la_CPPFLAGS = -I. -I$(switch_srcdir)/src/mod/endpoints/mod_verto/mcast
mod_verto_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_verto_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

if HAVE_PERL
#perldir = $(PERL_SITEDIR)
noinst_LTLIBRARIES = MCAST.la
MCAST_la_SOURCES   = mcast/mcast_wrap.cpp mcast/perlxsi.c mcast/mcast.c mcast/mcast_cpp.cpp
MCAST_la_CFLAGS    = $(CC_CFLAGS) $(CFLAGS) $(SWITCH_AM_CFLAGS) $(PERL_CFLAGS)
MCAST_la_CXXFLAGS  = $(SWITCH_AM_CXXFLAGS) $(CXXFLAGS) -w $(PERL_INC)
MCAST_la_CPPFLAGS  = -I$(switch_srcdir)/src/mod/endpoints/mod_verto/mcast
MCAST_la_LDFLAGS   = -avoid-version -module -no-undefined -shared $(PERL_LDFLAGS)

#install-data-local: perlmod-install

#perlmod-install: install-perlLTLIBRARIES
#	install -d -m 755 $(DESTDIR)$(PERL_SITEDIR)
#	install -m 755 mcast/MCAST.pm $(DESTDIR)$(PERL_SITEDIR)
endif

mcast/esl_wrap.cpp:
	cd mcast && swig -module MCAST -shadow -perl5 -c++ -DMULTIPLICITY -I../src/include -o mcast_wrap.cpp ../MCAST.i

mcast/perlxsi.c:
	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

clean-data-local:
	rm -f *.o *.so *~

swigclean:
	rm -f mcast/mcast_wrap.* mcast/MCAST.so mcast/MCAST.pm mcast/perlxsi.*

reswig:	swigclean mcast/mcast_wrap.cpp mcast/perlxsi.c
