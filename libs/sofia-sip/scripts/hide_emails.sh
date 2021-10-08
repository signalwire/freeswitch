#!/bin/sh
#
# description: Mangle email addresses in the HTML documentation
# author: Kai Vehmanen <kai.vehmanen@nokia.com>
# version: 20050908-3
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

echo "Postprocessing HTML in ${1:-.}: hiding email addresses, fixing links"

find ${1:-.} -name '*.html' -print0 |
xargs -0 \
sed -r -i '
# Hide e-mail addresses
s/([:>;][a-z][-a-z.]*)(@[a-z][a-z]*)\.[a-z][a-z]*(["<\&])/\1\2-email.address.hidden\3/gi;
# Fix cross-module links
s!doxygen="([a-z]*).doxytags:../\1/" href="../\1/\1_index.html"!doxygen="\1.doxytags:../\1/" href="../\1/index.html"!g;
'
