ACLOCAL_AMFLAGS=-I build
EXTRA_DIST =
SUBDIRS = . test
AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS    += -I$(top_srcdir)/src -I$(top_srcdir)/src/include -I$(top_srcdir)/crypt -O0
AM_CPPFLAGS  = $(AM_CFLAGS)

lib_LTLIBRARIES	  = libks.la
libks_la_SOURCES  = src/ks.c src/ks_string.c src/ks_json.c src/cJSON.c src/cJSON_Utils.c src/ks_thread.c src/ks_thread_pool.c src/ks_mutex.c src/ks_config.c
libks_la_SOURCES += src/ks_log.c src/ks_socket.c src/ks_buffer.c src/ks_pool.c src/simclist.c
libks_la_SOURCES += src/ks_time.c src/ks_printf.c src/ks_hash.c src/ks_q.c src/ks_dso.c # src/ks_dht.c
libks_la_SOURCES += src/ks_ssl.c src/kws.c src/ks_rng.c src/ks_base64.c
libks_la_SOURCES += src/utp/utp_api.cpp src/utp/utp_callbacks.cpp src/utp/utp_hash.cpp src/utp/utp_internal.cpp
libks_la_SOURCES += src/utp/utp_packedsockaddr.cpp src/utp/utp_utils.cpp src/ks_bencode.c
libks_la_SOURCES += src/dht/ks_dht.c src/dht/ks_dht_datagram.c src/dht/ks_dht_endpoint.c src/dht/ks_dht_message.c src/dht/ks_dht_transaction.c
libks_la_SOURCES += src/dht/ks_dht_job.c src/dht/ks_dht_search.c src/dht/ks_dht_publish.c src/dht/ks_dht_distribute.c src/dht/ks_dht_storageitem.c
libks_la_SOURCES += src/dht/ks_dht_bucket.c
libks_la_SOURCES += crypt/aeskey.c crypt/aestab.c crypt/sha2.c crypt/twofish.c crypt/aes_modes.c crypt/aescrypt.c crypt/twofish_cfb.c
#aes.h aescpp.h brg_endian.h aesopt.h aestab.h brg_types.h sha2.h twofish.h
libks_la_SOURCES += src/ks_acl.c

libks_la_CFLAGS	  = $(AM_CFLAGS)
libks_la_CPPFLAGS = -DPOSIX
libks_la_LDFLAGS  = $(AM_LDFLAGS) -version-info 0:1:0 -lncurses -lpthread -lm

library_includedir	= $(prefix)/include
library_include_HEADERS = src/include/ks_config.h src/include/ks.h src/include/ks_threadmutex.h src/include/ks_json.h src/include/ks_buffer.h
library_include_HEADERS += src/include/ks_thread_pool.h src/include/ks_cJSON.h src/include/ks_cJSON_Utils.h
library_include_HEADERS += src/include/ks_pool.h src/include/simclist.h src/include/ks_time.h src/include/ks_q.h src/include/ks_socket.h
library_include_HEADERS += src/include/ks_dso.h src/include/ks_platform.h src/include/ks_types.h # src/include/ks_rng.h src/include/ks_dht.h
library_include_HEADERS += src/include/ks_printf.h src/include/ks_hash.h src/include/ks_ssl.h src/include/kws.h
library_include_HEADERS += src/include/ks_base64.h
library_include_HEADERS += src/utp/utp_internal.h src/utp/utp.h src/utp/utp_types.h src/utp/utp_callbacks.h src/utp/utp_templates.h
library_include_HEADERS += src/utp/utp_hash.h src/utp/utp_packedsockaddr.h src/utp/utp_utils.h src/include/ks_utp.h src/include/ks_acl.h

tests: libks.la
	$(MAKE) -C test tests
