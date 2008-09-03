#!/bin/sh
#
# V.42bis compression/decompression tests, as specified in V.56ter
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
# $Id: v42bis_tests.sh,v 1.5 2008/05/03 09:34:26 steveu Exp $
#

BASE=../test-data/itu/v56ter

./v42bis_tests ${BASE}/1.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/1.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/1X04.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/1X04.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/1X30.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/1X30.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/2.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/2.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/2X10.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/2X10.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/3.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/3.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/3X06.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/3X06.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/4.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/4.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/4X04.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/4X04.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/5.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/5.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
./v42bis_tests ${BASE}/5X16.TST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
diff ${BASE}/5X16.TST v42bis_tests.out
RETVAL=$?
if [ $RETVAL != 0 ]
then
    exit $RETVAL
fi
