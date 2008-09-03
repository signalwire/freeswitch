#!/bin/sh
#
# SpanDSP - a series of DSP components for telephony
#
# unpack_g726_data.sh
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 2.1,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: unpack_g726_data.sh,v 1.6 2008/05/03 07:55:04 steveu Exp $
#

ITUDATA="../../../T-REC-G.726-199103-I!AppII!SOFT-ZST-E.zip"

cd test-data/itu
if [ -d g726 ]
then
    cd g726
else
    mkdir g726
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/itu/g726!
        exit $RETVAL
    fi
    cd g726
fi

rm -rf DISK1
rm -rf DISK2
rm -rf G726piiE.WW7.doc
rm -rf Software.zip
unzip ${ITUDATA} >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for G.726!
    exit $RETVAL
fi
#rm $(ITUDATA}
rm G726piiE.WW7.doc
unzip Software.zip >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for G.726!
    exit $RETVAL
fi
rm Software.zip
mv ./software/G726ap2/G726ap2e/DISK1 .
mv ./software/G726ap2/G726ap2e/DISK2 .
rm -rf ./software
echo The ITU test vectors for G.726 should now be in the g726 directory
