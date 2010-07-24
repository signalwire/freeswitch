#!/bin/sh
#
# SpanDSP - a series of DSP components for telephony
#
# unpack_g722_data.sh
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

ITUDATA="../../../T-REC-G.722-198703-I!AppII!ZPF-E.zip"

cd test-data/itu
if [ -d g722 ]
then
    cd g722
else
    mkdir g722
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/itu/g722!
        exit $RETVAL
    fi
    cd g722
fi

rm -rf T*
rm -rf software
unzip ${ITUDATA} >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for G.722!
    exit $RETVAL
fi
#rm ${ITUDATA}
unzip ./software/G722ap2/G722E/Software.zip >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for G.722!
    exit $RETVAL
fi
mv ./software/G722ap2/G722E/T* .
rm -rf software
echo The ITU test vectors for G.722 should now be in the g722 directory
