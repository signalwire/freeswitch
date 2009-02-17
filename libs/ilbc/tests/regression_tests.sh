#!/bin/sh
#
# iLBC - a library for the iLBC codec
#
# regression_tests.sh
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: regression_tests.sh,v 1.1.1.1 2008/02/15 12:15:55 steveu Exp $
#

STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

echo Performing basic iLBC regression tests
echo

./ilbc_tests 20 ../localtests/iLBC.INP iLBC_20ms.BIT iLBC_20ms_clean.OUT ../localtests/clean.chn >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 20ms clean failed!
    exit $RETVAL
fi
diff iLBC_20ms.BIT ../localtests/iLBC_20ms.BIT
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 20ms clean failed!
    #exit $RETVAL
fi
diff iLBC_20ms_clean.OUT ../localtests/iLBC_20ms_clean.OUT
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 20ms clean failed!
    #exit $RETVAL
fi
echo ilbc_tests 20ms clean completed OK

./ilbc_tests 20 ../localtests/iLBC.INP tmp.BIT iLBC_20ms_tlm05.OUT ../localtests/tlm05.chn >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 20ms 5% loss failed!
    exit $RETVAL
fi
diff iLBC_20ms_tlm05.OUT ../localtests/iLBC_20ms_tlm05.OUT
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 20ms clean failed!
    #exit $RETVAL
fi
echo ilbc_tests 20ms 5% loss completed OK

./ilbc_tests 30 ../localtests/iLBC.INP iLBC_30ms.BIT iLBC_30ms_clean.OUT ../localtests/clean.chn >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 30ms clean failed!
    exit $RETVAL
fi
diff iLBC_30ms.BIT ../localtests/iLBC_30ms.BIT
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 30ms clean failed!
    #exit $RETVAL
fi
diff iLBC_30ms_clean.OUT ../localtests/iLBC_30ms_clean.OUT
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 30ms clean failed!
    #exit $RETVAL
fi
echo ilbc_tests 30ms clean completed OK

./ilbc_tests 30 ../localtests/iLBC.INP tmp.BIT iLBC_30ms_tlm05.OUT ../localtests/tlm05.chn >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 30ms 5% loss failed!
    exit $RETVAL
fi
diff iLBC_30ms_tlm05.OUT ../localtests/iLBC_30ms_tlm05.OUT
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ilbc_tests 30ms clean failed!
    #exit $RETVAL
fi
echo ilbc_tests 30ms 5% loss completed OK

echo
echo All regression tests successfully completed
