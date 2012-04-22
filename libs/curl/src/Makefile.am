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
# $Id: Makefile.am,v 1.54 2006-08-07 18:06:37 yangtse Exp $
###########################################################################
AUTOMAKE_OPTIONS = foreign nostdinc

# we use srcdir/src for "private" includes
# we use srcdir/include for the static global include files
# we use builddir/src for the generated include files to get found
# we use srcdir/lib for the header files we "borrow" from the lib

INCLUDES = -I$(top_srcdir)/include \
           -I$(top_builddir)/lib   \
           -I$(top_builddir)/src   \
           -I$(top_srcdir)/lib     \
           -I$(top_srcdir)/src

include Makefile.inc

EXTRA_DIST = mkhelp.pl makefile.dj Makefile.vc6 Makefile.b32 Makefile.m32 \
	Makefile.riscos config.h.in macos/curl.mcp.xml.sit.hqx		  \
	macos/MACINSTALL.TXT macos/src/curl_GUSIConfig.cpp		  \
	macos/src/macos_main.cpp config-amigaos.h makefile.amiga curl.rc  \
	Makefile.netware Makefile.inc Makefile.Watcom

