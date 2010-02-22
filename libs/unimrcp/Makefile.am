macrodir		= @ac_macro_dir@
auxdir			= @ac_aux_dir@
AUX_DIST		= $(auxdir)/config.guess \
                          $(auxdir)/config.sub \
                          $(auxdir)/install-sh \
                          $(auxdir)/ltconfig \
                          $(auxdir)/ltmain.sh \
                          $(auxdir)/depcomp \
                          $(auxdir)/missing
EXTRA_DIST		= bootstrap

AUTOMAKE_OPTIONS 	= foreign
MAINTAINERCLEANFILES 	= Makefile.in aclocal.m4 configure $(AUX_DIST)

ACLOCAL			= aclocal -I $(macrodir)

SUBDIRS                 = conf data libs modules plugins platforms build
if TEST_SUITES
SUBDIRS                += tests
endif

dox:
	doxygen $(top_srcdir)/docs/doxygen.conf
 
install-data-local:
	test -d $(logdir)  || $(mkinstalldirs) $(logdir)
