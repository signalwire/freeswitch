#
# Makefile.am for sofia-sip/packages
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# Install pkg-config file here
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sofia-sip-ua.pc

if HAVE_GLIB
# We should probably have something here telling
# if we have gobject support or not
pkgconfig_DATA += sofia-sip-ua-glib.pc
endif

EXTRA_DIST = sofia-sip-${PACKAGE_VERSION}.spec sofia-sip.spec.in
EXTRA_DIST += sofia-sip-ua.pc.in sofia-sip-ua-glib.pc.in

DISTCLEANFILES = sofia-sip-*.spec sofia-sip.spec
