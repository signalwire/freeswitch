#
# Makefile.am @template@ for su module
#
# Copyright (C) 2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#

# ----------------------------------------------------------------------
# Headers

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libsu.la

bin_PROGRAMS = 		addrinfo localinfo

check_PROGRAMS = 	torture_su torture_su_port \
			torture_su_alloc torture_su_time torture_su_tag \
			test_htable test_htable2 torture_rbtree torture_heap \
			test_memmem torture_su_bm \
			torture_su_root torture_su_timer \
			test_su su_proxy test_poll $(OSXPROGS)

# ----------------------------------------------------------------------
# Tests

TESTS = 		torture_su torture_su_port \
			torture_su_alloc torture_su_time torture_su_tag \
			test_htable test_htable2 torture_rbtree torture_heap \
			test_memmem torture_su_bm \
			torture_su_root torture_su_timer \
			run_addrinfo run_localinfo run_test_su \
			$(OSXTESTS)

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES = su_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/su_types.h sofia-sip/su.h \
			sofia-sip/su_errno.h sofia-sip/su_addrinfo.h \
			sofia-sip/su_localinfo.h sofia-sip/su_wait.h \
			sofia-sip/su_alloc.h sofia-sip/su_alloc_stat.h \
			sofia-sip/su_strlst.h sofia-sip/su_vector.h \
			sofia-sip/su_time.h sofia-sip/su_tag.h \
			sofia-sip/su_tag_class.h sofia-sip/su_tagarg.h \
			sofia-sip/su_tag_io.h sofia-sip/su_tag_inline.h \
			sofia-sip/htable.h sofia-sip/htable2.h \
			sofia-sip/rbtree.h sofia-sip/heap.h \
			sofia-sip/su_debug.h sofia-sip/su_log.h \
			sofia-sip/su_config.h sofia-sip/su_md5.h \
			sofia-sip/su_uniqueid.h sofia-sip/su_bm.h \
			sofia-sip/tstdef.h sofia-sip/su_os_nw.h \
			sofia-sip/su_string.h sofia-sip/string0.h \
			$(OSXHEADERS)

nobase_nodist_include_sofia_HEADERS = sofia-sip/su_configure.h

libsu_la_SOURCES = \
	su.c su_errno.c su_addrinfo.c \
	su_alloc.c su_alloc_lock.c su_strdup.c su_sprintf.c \
	su_strlst.c su_vector.c \
	su_time.c su_time0.c \
	su_wait.c su_root.c su_timer.c \
	su_port.c  su_port.h \
	su_base_port.c su_pthread_port.c su_socket_port.c \
	su_poll_port.c su_epoll_port.c su_select_port.c su_kqueue_port.c \
	su_devpoll_port.c \
	su_localinfo.c \
	su_os_nw.c \
	su_taglist.c su_tag.c su_tag_io.c \
	su_log.c su_global_log.c su_default_log.c su_module_debug.h \
	su_md5.c su_uniqueid.c su_bm.c smoothsort.c su_string.c string0.c \
	$(OSXSOURCES)


EXTRA_libsu_la_SOURCES = \
			memmem.c strtoull.c \
			memspn.c memcspn.c memccpy.c \
			inet_ntop.c inet_pton.c poll.c getopt.c \
			su_tag_ref.c

# Automake 1.9 complains about missing CXX
EXTRA_DIST =		su_win32_port.c \
			su_open_c_localinfo.cpp

libsu_la_LIBADD = 	$(REPLACE_LIBADD)
libsu_la_DEPENDENCIES = $(REPLACE_LIBADD)

COVERAGE_INPUT = 	$(libsu_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libsu.la

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST += 		su.docs \
			run_addrinfo run_localinfo run_test_su

dist_pkgdata_SCRIPTS = 	tag_dll.awk

sofia-sip/su_configure.h: sofia-sip/su_configure.h.in

# ----------------------------------------------------------------------
# Automake options

AUTOMAKE_OPTIONS = 	foreign

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

# ----------------------------------------------------------------------
# OS X stuff

if COREFOUNDATION
OSXSOURCES =  su_osx_runloop.c
OSXHEADERS =  sofia-sip/su_osx_runloop.h
OSXPROGS =    torture_su_root_osx test_su_osx
OSXTESTS =    torture_su_root_osx run_test_su_osx
endif

EXTRA_DIST += su_osx_runloop.c sofia-sip/su_osx_runloop.h
