#!/bin/sh

TESTDATADIR=../test-data/broadcom/fixed/bv32

# Clean
if test -f tv.bv32
then
\rm tv.bv32
fi
if test -f tv.bv32.raw
then
\rm tv.bv32.raw
fi
if test -f tv.bv32.bfe10.raw
then
\rm tv.bv32.bfe10.raw
fi

# Set error pattern files
./bv32_tests enc ${TESTDATADIR}/tv.raw tv.bv32
./bv32_tests dec ${TESTDATADIR}/tv.bv32.ref tv.bv32.raw
./bv32_tests dec ${TESTDATADIR}/tv.bfe10.bv32 tv.bv32.bfe10.raw

checksum=0;
if test -n "`cmp tv.bv32 ${TESTDATADIR}/tv.bv32.ref`"
then
checksum=`expr $checksum + 1`
fi
if test -n "`cmp tv.bv32.raw ${TESTDATADIR}/tv.bv32.ref.raw`"
then
checksum=`expr $checksum + 1`
fi
if test -n "`cmp tv.bv32.bfe10.raw ${TESTDATADIR}/tv.bv32.bfe10.ref.raw`"
then
checksum=`expr $checksum + 1`
fi

if test $checksum -eq 0
then
echo "  **************************************************************************"
echo "  * CONGRATULATIONS: Your compilation passed the simple functionality test *"
echo "  **************************************************************************"
echo ""
\rm tv.bv32 tv.bv32.raw tv.bv32.bfe10.raw
else
echo "  ************************************************************************"
echo "  * WARNING: Your compilation DID NOT pass the simple functionality test *"
echo "  ************************************************************************"
echo ""
fi
