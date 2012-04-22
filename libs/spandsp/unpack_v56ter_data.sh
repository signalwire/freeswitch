#!/bin/sh
#
# SpanDSP - a series of DSP components for telephony
#
# unpack_v56ter_data.sh
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

ITUDATA="../../../T-REC-V.56ter-199608-I!!ZPF-E.zip"

cd test-data/itu
if [ -d v56ter ]
then
    cd v56ter
else
    mkdir v56ter
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/itu/v56ter!
        exit $RETVAL
    fi
    cd v56ter
fi

rm -rf software
rm -rf *.TST
unzip ${ITUDATA} >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for V.56ter!
    exit $RETVAL
fi
#rm ${ITUDATA}
unzip software/V56ter/V56tere/Software.zip >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for V.56ter!
    exit $RETVAL
fi
mv ./software/V56ter/V56tere/*.TST .
chmod 644 *.TST
rm -rf software
echo The ITU test vectors for V.56ter should now be in the v56ter directory
