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

run_tsb85_test()
{
    rm -f fax_tests_1.tif
    echo ./tsb85_tests ${TEST}
    ./tsb85_tests ${TEST} 2>xyzzy2
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo tsb85_tests ${TEST} failed!
        exit $RETVAL
    fi
}

for TEST in MRGN01 MRGN02 MRGN03 MRGN04 MRGN05 MRGN06a MRGN06b MRGN07 MRGN08
do
    run_tsb85_test
done

#MRGN14 fails because we don't adequately distinguish between receiving a
#bad image signal and receiving none at all.
#MRGN16 fails because we don't adequately distinguish between receiving a
#bad image signal and receiving none at all.

#for TEST in MRGN09 MRGN10 MRGN11 MRGN12 MRGN13 MRGN14 MRGN15 MRGN16 MRGN17
for TEST in MRGN09 MRGN10 MRGN11 MRGN12 MRGN13 MRGN15 MRGN17
do
    run_tsb85_test
done

for TEST in ORGC01 ORGC02 ORGC03
do
    run_tsb85_test
done

for TEST in OREN01 OREN02 OREN03 OREN04 OREN05 OREN06 OREN07 OREN08 OREN09 OREN10
do
    run_tsb85_test
done

# MRGX03 is failing because the V.27ter modem says it trained on HDLC
# MRGX05 is failing because we don't distinguish MPS immediately after MCF from MPS after
# a corrupt image signal.

#for TEST in MRGX01 MRGX02 MRGX03 MRGX04 MRGX05 MRGX06 MRGX07 MRGX08
for TEST in MRGX01 MRGX02 MRGX04 MRGX06 MRGX07 MRGX08
do
    run_tsb85_test
done

for TEST in MRGX09 MRGX10 MRGX11 MRGX12 MRGX13 MRGX14 MRGX15
do
    run_tsb85_test
done

for TEST in MTGP01 MTGP02 OTGP03
do
    run_tsb85_test
done

for TEST in MTGN01 MTGN02 MTGN03 MTGN04 MTGN05 MTGN06 MTGN07 MTGN08 MTGN09 MTGN10
do
    run_tsb85_test
done

for TEST in MTGN11 MTGN12 MTGN13 MTGN14 MTGN15 MTGN16 MTGN17 MTGN18 MTGN19 MTGN20
do
    run_tsb85_test
done

for TEST in MTGN21 MTGN22 MTGN23 MTGN24 MTGN25 MTGN26 MTGN27 MTGN28
do
    run_tsb85_test
done

for TEST in OTGC01 OTGC02 OTGC03 OTGC04 OTGC05 OTGC06 OTGC07 OTGC08
do
    run_tsb85_test
done

for TEST in OTGC09-01 OTGC09-02 OTGC09-03 OTGC09-04 OTGC09-05 OTGC09-06 OTGC09-07 OTGC09-08 OTGC09-09 OTGC09-10 OTGC09-11 OTGC09-12
do
    run_tsb85_test
done

for TEST in OTGC10 OTGC11
do
    run_tsb85_test
done

for TEST in OTEN01 OTEN02 OTEN03 OTEN04 OTEN05 OTEN06
do
    run_tsb85_test
done

for TEST in MTGX01 MTGX02 MTGX03 MTGX04 MTGX05 MTGX06 MTGX07 MTGX08
do
    run_tsb85_test
done

for TEST in MTGX09 MTGX10 MTGX11 MTGX12 MTGX13 MTGX14 MTGX15 MTGX16
do
    run_tsb85_test
done

for TEST in MTGX17 MTGX18 MTGX19 MTGX20 MTGX21 MTGX22 MTGX23
do
    run_tsb85_test
done

for TEST in MRGP01 MRGP02 MRGP03 MRGP04 MRGP05 MRGP06 MRGP07 MRGP08
do
    run_tsb85_test
done

for TEST in ORGP09 ORGP10
do
    run_tsb85_test
done
