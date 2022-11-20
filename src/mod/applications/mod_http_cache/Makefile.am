include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_http_cache

noinst_LTLIBRARIES = libhttpcachemod.la
libhttpcachemod_la_SOURCES  = mod_http_cache.c common.c aws.c azure.c

mod_LTLIBRARIES = mod_http_cache.la
mod_http_cache_la_SOURCES  = 
mod_http_cache_la_CFLAGS   = $(AM_CFLAGS)
mod_http_cache_la_CPPFLAGS = $(CURL_CFLAGS) $(AM_CPPFLAGS)
mod_http_cache_la_LIBADD   = $(switch_builddir)/libfreeswitch.la libhttpcachemod.la
mod_http_cache_la_LDFLAGS  = $(CURL_LIBS) -avoid-version -module -no-undefined -shared

noinst_PROGRAMS = test/test_aws

test_test_aws_SOURCES = test/test_aws.c
test_test_aws_CFLAGS = $(AM_CFLAGS) -I. -DSWITCH_TEST_BASE_DIR_FOR_CONF=\"${abs_builddir}/test\" -DSWITCH_TEST_BASE_DIR_OVERRIDE=\"${abs_builddir}/test\"
test_test_aws_LDFLAGS = $(AM_LDFLAGS) -avoid-version -no-undefined $(freeswitch_LDFLAGS) $(switch_builddir)/libfreeswitch.la $(CORE_LIBS) $(APR_LIBS)
test_test_aws_LDADD = libhttpcachemod.la

TESTS = $(noinst_PROGRAMS)

