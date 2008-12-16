#! /bin/gawk
#
# This script extracts the version information from configure.ac
# and re-generates win32/config.h and
# libsofia-sip-ua/features/sofia_sip_features.h
#
# --------------------------------------------------------------------
#
# This file is part of the Sofia-SIP package
#
# Copyright (C) 2005 Nokia Corporation.
#
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA
#
# --------------------------------------------------------------------
#
# Contributor(s): Pekka.Pessi@nokia.com.
#
# Created: Wed Jan 25 15:57:10 2006 ppessi
#

BEGIN { IN=1; OUT=0; }

IN && /^AC_INIT/ { version=$2; gsub(/[\]\[)]/, "", version); }

OUT && /@[A-Z_]+@/ {
  gsub(/@PACKAGE_VERSION@/, version);
  gsub(/@PACKAGE_BUGREPORT@/, "sofia-sip-devel@lists.sourceforge.net");
  gsub(/@PACKAGE_NAME@/, "sofia-sip");
  gsub(/@PACKAGE@/, "sofia-sip");
  gsub(/@PACKAGE_STRING@/, "sofia-sip");
  gsub(/@PACKAGE_TARNAME@/, "sofia-sip");
}

OUT { print; }

