##
## Process this file with automake to produce Makefile.in
##

INCLUDES = -I$(top_srcdir)/include

TESTS = tst-ikstack tst-iks tst-sax tst-dom tst-sha tst-md5 tst-filter tst-jid

noinst_PROGRAMS = tst-ikstack tst-iks tst-sax tst-dom tst-sha tst-md5 tst-filter tst-jid

tst_ikstack_LDADD = $(top_builddir)/src/libiksemel.la
tst_ikstack_SOURCES = tst-ikstack.c

tst_iks_LDADD = $(top_builddir)/src/libiksemel.la
tst_iks_SOURCES = tst-iks.c

tst_sax_LDADD = $(top_builddir)/src/libiksemel.la
tst_sax_SOURCES = tst-sax.c

tst_dom_LDADD = $(top_builddir)/src/libiksemel.la
tst_dom_SOURCES = tst-dom.c

tst_sha_LDADD = $(top_builddir)/src/libiksemel.la
tst_sha_SOURCES = tst-sha.c

tst_md5_LDADD = $(top_builddir)/src/libiksemel.la
tst_md5_SOURCES = tst-md5.c

tst_filter_LDADD = $(top_builddir)/src/libiksemel.la
tst_filter_SOURCES = tst-filter.c

tst_jid_LDADD = $(top_builddir)/src/libiksemel.la
tst_jid_SOURCES = tst-jid.c
