#!/bin/sh
#
# VoIPcodecs - a series of DSP components for telephony
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
# $Id: regression_tests.sh,v 1.47 2007/12/22 12:37:22 steveu Exp $
#

STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

echo Performing basic VoIP codecs regression tests
echo

./g711_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo g711_tests failed!
    exit $RETVAL
fi
echo g711_tests completed OK

./g722_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo g722_tests failed!
    exit $RETVAL
fi
echo g722_tests completed OK

./g726_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo g726_tests failed!
    exit $RETVAL
fi
echo g726_tests completed OK

./gsm0610_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo gsm0610_tests failed!
    exit $RETVAL
fi
echo gsm0610_tests completed OK

./ima_adpcm_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo ima_adpcm_tests failed!
    exit $RETVAL
fi
echo ima_adpcm_tests completed OK

./lpc10_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo lpc10_tests failed!
    exit $RETVAL
fi
echo lpc10_tests completed OK

./oki_adpcm_tests >$STDOUT_DEST 2>$STDERR_DEST
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo oki_adpcm_tests failed!
    exit $RETVAL
fi
echo oki_adpcm_tests completed OK

echo
echo All regression tests successfully completed
