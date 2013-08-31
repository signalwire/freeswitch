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
    rm -f fax_tests.tif
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
    ${TIFFCMP} -t ${FILE} fax_tests.tif >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    rm -f fax_tests.tif
    echo tested ${FILE}
}

run_fax_squash_test()
{
    # Test with lengthwise squashing of a bilevel image
    rm -f fax_tests.tif
    echo ./fax_tests -b ${SQ} -b ${SQ} ${OPTS} -i ${IN_FILE}
    ./fax_tests -b ${SQ} ${OPTS} -i ${IN_FILE} >xyzzy 2>xyzzy2
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    # Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
    # option means the normal differences in tags will be ignored.
    ${TIFFCMP} -t ${OUT_FILE} fax_tests.tif >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    rm -f fax_tests.tif
    echo tested ${FILE}
}

run_colour_fax_test()
{
    rm -f fax_tests.tif
    echo ./fax_tests ${OPTS} -i ${IN_FILE}
    ./fax_tests ${OPTS} -i ${IN_FILE} >xyzzy 2>xyzzy2
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    # Now use tiffcmp to check the results. It will return non-zero if any page images differ. The -t
    # option means the normal differences in tags will be ignored.
    ${TIFFCMP} -t ${OUT_FILE} fax_tests.tif >/dev/null
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo fax_tests failed!
        exit $RETVAL
    fi
    rm -f fax_tests.tif
    echo tested ${IN_FILE} to ${OUT_FILE}
}

ITUTESTS_DIR=../test-data/itu/fax
TIFFFX_DIR=../test-data/itu/tiff-fx
LOCALTESTS_DIR=../test-data/local
TIFFCMP=tiffcmp

# Colour/gray -> bilevel by not allowing ECM
for OPTS in "-p AA" "-p TT" "-p GG" "-p TG" "-p GT"
do
    IN_FILE="${LOCALTESTS_DIR}/lenna-colour.tif"
    OUT_FILE="${LOCALTESTS_DIR}/lenna-colour-bilevel.tif"
    run_colour_fax_test

    IN_FILE="${LOCALTESTS_DIR}/lenna-bw.tif"
    OUT_FILE="${LOCALTESTS_DIR}/lenna-bw-bilevel.tif"
    run_colour_fax_test

    IN_FILE="${TIFFFX_DIR}/c03x_02x.tif"
    OUT_FILE="${TIFFFX_DIR}/c03x_02x-bilevel.tif"
    run_colour_fax_test

    IN_FILE="${TIFFFX_DIR}/l02x_02x.tif"
    OUT_FILE="${TIFFFX_DIR}/l02x_02x-bilevel.tif"
    run_colour_fax_test

    IN_FILE="${TIFFFX_DIR}/l04x_02x.tif"
    OUT_FILE="${TIFFFX_DIR}/l04x_02x-bilevel.tif"
    run_colour_fax_test
done

# Colour/gray -> colour/gray
for OPTS in "-p AA -C -e" "-p TT -C -e" "-p GG -C -e" "-p TG -C -e" "-p GT -C -e"
do
#    IN_FILE="${LOCALTESTS_DIR}/lenna-colour.tif"
#    OUT_FILE="${LOCALTESTS_DIR}/lenna-colour-out.tif"
#    run_colour_fax_test

#    IN_FILE="${LOCALTESTS_DIR}/lenna-bw.tif"
#    OUT_FILE="${LOCALTESTS_DIR}/lenna-bw-out.tif"
#    run_colour_fax_test

#    IN_FILE="${TIFFFX_DIR}/c03x_02x.tif"
#    OUT_FILE="${TIFFFX_DIR}/c03x_02x-out.tif"
#    run_colour_fax_test

    IN_FILE="${TIFFFX_DIR}/l02x_02x.tif"
    OUT_FILE="${TIFFFX_DIR}/l02x_02x.tif"
    run_colour_fax_test

#    IN_FILE="${TIFFFX_DIR}/l04x_02x.tif"
#    OUT_FILE="${TIFFFX_DIR}/l04x_02x.tif"
#    run_colour_fax_test
done

# Bi-level tests with image squashing
for OPTS in "-p AA" "-p AA -e" "-p TT" "-p TT -e" "-p GG" "-p GG -e" "-p TG" "-p TG -e" "-p GT" "-p GT -e"
do
    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_77_A4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_77SQ_A4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_77_B4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_77SQ_B4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_77_A3.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_77SQ_A3.tif"
    SQ=4
    run_fax_squash_test


    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_154_A4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_154SQ_A4.tif"
    SQ=3
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_154_B4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_154SQ_B4.tif"
    SQ=3
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_154_A3.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_154SQ_A3.tif"
    SQ=3
    run_fax_squash_test


    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_154_A4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_154SQSQ_A4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_154_B4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_154SQSQ_B4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_R8_154_A3.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_R8_154SQSQ_A3.tif"
    SQ=4
    run_fax_squash_test


    IN_FILE="${ITUTESTS_DIR}/bilevel_200_200_A4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_200SQ_A4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_200_200_B4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_200SQ_B4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_200_200_A3.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_200SQ_A3.tif"
    SQ=4
    run_fax_squash_test


    IN_FILE="${ITUTESTS_DIR}/bilevel_200_400_A4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_400SQ_A4.tif"
    SQ=3
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_200_400_B4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_400SQ_B4.tif"
    SQ=3
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_200_400_A3.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_400SQ_A3.tif"
    SQ=3
    run_fax_squash_test


    IN_FILE="${ITUTESTS_DIR}/bilevel_200_400_A4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_400SQSQ_A4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_200_400_B4.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_400SQSQ_B4.tif"
    SQ=4
    run_fax_squash_test

    IN_FILE="${ITUTESTS_DIR}/bilevel_200_400_A3.tif"
    OUT_FILE="${ITUTESTS_DIR}/bilevel_200_400SQSQ_A3.tif"
    SQ=4
    run_fax_squash_test
done

# Bi-level tests
for OPTS in "-p AA" "-p AA -e" "-p TT" "-p TT -e" "-p GG" "-p GG -e" "-p TG" "-p TG -e" "-p GT" "-p GT -e"
do
    FILE="${ITUTESTS_DIR}/itutests.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/100pages.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/striped.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/mixed_size_pages.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_R8_385_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R8_385_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R8_385_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_R8_77_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R8_77_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R8_77_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_R8_154_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R8_154_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R8_154_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_R16_154_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R16_154_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_R16_154_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_200_100_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_200_100_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_200_100_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_200_200_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_200_200_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_200_200_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_200_400_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_200_400_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_200_400_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_300_300_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_300_300_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_300_300_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_300_600_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_300_600_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_300_600_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_400_400_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_400_400_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_400_400_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_400_800_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_400_800_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_400_800_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_600_600_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_600_600_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_600_600_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_600_1200_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_600_1200_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_600_1200_A3.tif"
    run_fax_test


    FILE="${ITUTESTS_DIR}/bilevel_1200_1200_A4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_1200_1200_B4.tif"
    run_fax_test

    FILE="${ITUTESTS_DIR}/bilevel_1200_1200_A3.tif"
    run_fax_test
done

echo
echo All fax tests successfully completed
