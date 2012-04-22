#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) 1998 - 2005, Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at http://curl.haxx.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# $Id: Makefile.am,v 1.56 2005-11-16 07:20:58 bagder Exp $
###########################################################################

AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = CHANGES COPYING maketgz reconf Makefile.dist curl-config.in \
 curl-style.el sample.emacs RELEASE-NOTES buildconf buildconf.bat libcurl.pc.in

bin_SCRIPTS = curl-config

SUBDIRS = lib src
DIST_SUBDIRS = $(SUBDIRS) include packages

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcurl.pc

dist-hook:
	find $(distdir) -name "*.dist" -exec rm {} \;
	(distit=`find $(srcdir) -name "*.dist"`; \
	for file in $$distit; do \
	  strip=`echo $$file | sed -e s/^$(srcdir)// -e s/\.dist//`; \
	  cp $$file $(distdir)$$strip; \
	done)

# We extend the standard install with a custom hook:
install-data-hook:
	cd include && $(MAKE) install

# We extend the standard uninstall with a custom hook:
uninstall-hook:
	cd include && $(MAKE) uninstall

