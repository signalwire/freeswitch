MAINTAINERCLEANFILES = Makefile.in

DATAFILES            =

if COMMON_CLIENT_DATA
DATAFILES           += grammar.jsgf grammar.mixed grammar.srgs grammar.xml \
                       speak.txt speak.xml params_default.txt \
                       one-16kHz.pcm one-8kHz.pcm \
                       johnsmith-16kHz.pcm johnsmith-8kHz.pcm
endif

if DEMOSYNTH_PLUGIN
DATAFILES           += demo-16kHz.pcm demo-8kHz.pcm
endif

if DEMORECOG_PLUGIN
DATAFILES           += result.xml
endif

if DEMOVERIFIER_PLUGIN
DATAFILES           += result-verification.xml
endif

def-data:
	test -d $(DESTDIR)$(datadir) || $(mkinstalldirs) $(DESTDIR)$(datadir)
	for datafile in $(DATAFILES) ; do \
		$(INSTALL) -m 644 $(top_srcdir)/data/$$datafile $(DESTDIR)$(datadir); \
	done

install-data-local:
	test -d $(DESTDIR)$(datadir) || $(MAKE) def-data
