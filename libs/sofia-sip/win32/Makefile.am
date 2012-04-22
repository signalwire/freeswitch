#
# Makefile.am for sofia-sip/win32
#
# This is used for including win32 files in dist
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#
# ref: http://www.gnu.org/software/automake/manual/automake.html

# ----------------------------------------------------------------------

noinst_HEADERS = sofia-sip/su_configure.h unistd.h config.h

EXTRA_DIST = SofiaSIP.dsw \
	libsofia-sip-ua/libsofia_sip_ua.dsp \
	libsofia-sip-ua/sofia-sip-ua.def \
	libsofia-sip-ua-static/libsofia_sip_ua_static.dsp \
	tests/test_nua/test_nua.dsp \
	tests/test_nua/test_nat_tags.cpp \
	tests/test_nta/test_nta.dsp \
	tests/test_tport/test_tport.dsp \
	tests/test_tport/test_class.cpp \
	tests/test_tport/test_table.cpp \
	tests/torture_su_alloc/torture_su_alloc.dsp \
	tests/torture_su_root/torture_su_root.dsp \
	tests/torture_su_tag/torture_su_tag.dsp \
	tests/test_su/test_su.dsp \
	tests/torture_su_time/torture_su_time.dsp \
	tests/torture_su_timer/torture_su_timer.dsp \
	tests/torture_su/torture_su.dsp \
	tests/test_memmem/test_memmem.dsp \
	tests/test_htable/test_htable.dsp \
	tests/torture_rbtree/torture_rbtree.dsp \
	tests/torture_su_bm/torture_su_bm.dsp \
	tests/torture_su_port/torture_su_port.dsp \
	utils/localinfo/localinfo.dsp \
	utils/sip_options/sip_options.dsp \
	utils/sip_options_static/sip_options_static.dsp \
	utils/sip_dig/sip_dig.dsp \
	utils/stunc/stunc.dsp \
	$(PTHREAD_DIST) \
	README.txt \
	autogen.cmd build_sources.cmd version_files.cmd version.awk \
	install.cmd check.cmd

# VC2005 Project files
EXTRA_DIST += SofiaSIP.sln \
    libsofia-sip-ua-static/libsofia_sip_ua_static.vcproj \
    libsofia-sip-ua/libsofia_sip_ua.vcproj \
    tests/test_htable/test_htable.vcproj \
    tests/test_memmem/test_memmem.vcproj \
    tests/test_nta/test_nta.vcproj \
    tests/test_nua/test_nua.vcproj \
    tests/test_su/test_su.vcproj \
    tests/test_tport/test_tport.vcproj \
    tests/torture_rbtree/torture_rbtree.vcproj \
    tests/torture_su/torture_su.vcproj \
    tests/torture_su_alloc/torture_su_alloc.vcproj \
    tests/torture_su_bm/torture_su_bm.vcproj \
    tests/torture_su_port/torture_su_port.vcproj \
    tests/torture_su_root/torture_su_root.vcproj \
    tests/torture_su_tag/torture_su_tag.vcproj \
    tests/torture_su_time/torture_su_time.vcproj \
    tests/torture_su_timer/torture_su_timer.vcproj \
    utils/localinfo/localinfo.vcproj \
    utils/sip_dig/sip_dig.vcproj \
    utils/sip_options/sip_options.vcproj \
    utils/sip_options_static/sip_options_static.vcproj \
    utils/stunc/stunc.vcproj

PTHREAD_DIST = \
	pthread/ChangeLog \
	pthread/md5.sum.txt \
	pthread/pthreadVC2.dll \
	pthread/pthreadVC2.lib \
	pthread/semaphore.h \
	pthread/sched.h \
	pthread/pthread.h

mostlyclean-local:
	test "$(top_builddir)" = "$(top_srcdir)" || rm -f config.h
