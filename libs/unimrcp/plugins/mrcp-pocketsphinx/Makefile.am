MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -Iinclude \
                              -I$(top_srcdir)/libs/mrcp-engine/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) \
                             $(UNIMRCP_APU_INCLUDES) \
                             $(UNIMRCP_SPHINXBASE_INCLUDES) \
                             $(UNIMRCP_POCKETSPHINX_INCLUDES)

plugin_LTLIBRARIES          = mrcppocketsphinx.la

mrcppocketsphinx_la_SOURCES = src/mrcp_pocketsphinx.c \
                              src/pocketsphinx_properties.c
mrcppocketsphinx_la_LDFLAGS = -module $(PLUGIN_LT_VERSION)
mrcppocketsphinx_la_LIBADD  = $(UNIMRCP_POCKETSPHINX_LIBS) $(UNIMRCP_SPHINXBASE_LIBS) -lm


dictionary:
	!(test -f $(UNIMRCP_POCKETSPHINX_MODELS)/lm/cmudict.0.6d) || \
		$(INSTALL) -m 644 $(UNIMRCP_POCKETSPHINX_MODELS)/lm/cmudict.0.6d $(datadir)/default.dic

model:
	!(test -d $(UNIMRCP_POCKETSPHINX_MODELS)/hmm/wsj1/) || ($(mkinstalldirs) $(datadir)/wsj1; \
		$(INSTALL) -m 644 $(UNIMRCP_POCKETSPHINX_MODELS)/hmm/wsj1/* $(datadir)/wsj1)

install-data-local:
	test -d $(confdir) || $(mkinstalldirs) $(confdir)
	test -f $(confdir)/pocketsphinx.xml || $(INSTALL) -m 644 conf/pocketsphinx.xml $(confdir)
	test -f $(datadir)/default.dic || $(MAKE) dictionary
	test -d $(datadir)/wsj1 || $(MAKE) model
