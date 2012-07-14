#!/bin/sh
#
# spandsp fax tests
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

run_fax_test()
{
    rm -f fax_tests_1.tif
    echo ./fax_tests ${OPTS} -i ${FILE}
    ./fax_tests ${OPTS} -i ${FILE} >xyzzy 2>xyzzy2
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    # Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
    # option means the normal differences in tags will be ignored.
    tiffcmp -t ${FILE} fax_tests.tif >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    rm -f fax_tests_1.tif
    echo tested ${FILE}
}

ITUTESTS_DIR=../test-data/itu/fax

for OPTS in "-p AA" "-p AA -e" "-p TT" "-p TT -e" "-p GG" "-p GG -e" "-p TG" "-p TG -e" "-p GT" "-p GT -e"
do
    FILE="${ITUTESTS_DIR}/R8_385_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R8_385_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R8_385_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R8_77_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R8_77_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R8_77_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R8_154_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R8_154_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R8_154_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R300_300_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R300_300_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R300_300_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R300_600_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R300_600_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R300_600_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R16_154_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R16_154_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R16_154_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R16_800_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R16_800_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R16_800_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R600_600_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R600_600_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R600_600_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R600_1200_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R600_1200_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R600_1200_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/R1200_1200_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R1200_1200_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/R1200_1200_A3.tif"
    run_fax_test
done

echo
echo All fax tests successfully completed
