#!/bin/sh

TESTDATADIR=../test-data/broadcom/fixed/bv16

# Clean
if test -f tv.bv16
then
\rm tv.bv16
fi
if test -f tv.bv16.raw
then
\rm tv.bv16.raw
fi
if test -f bit.enc.tmp
then
\rm bit.enc.tmp
fi
if test -f bit.dec.tmp
then
\rm bit.dec.tmp
fi
if test -f tv.bv16.bfe10.raw
then
\rm tv.bv16.bfe10.raw
fi

# Set error pattern files
./bv16_tests enc ${TESTDATADIR}/tv.raw tv.bv16
./bv16_tests dec ${TESTDATADIR}/tv.bv16.ref tv.bv16.raw
./bv16_tests dec ${TESTDATADIR}/tv.bfe10.bv16 tv.bv16.bfe10.raw

checksum=0;
if test -n "`cmp tv.bv16 ${TESTDATADIR}/tv.bv16.ref`"
then
checksum=`expr $checksum + 1`
fi
if test -n "`cmp tv.bv16.raw ${TESTDATADIR}/tv.bv16.ref.raw`"
then
checksum=`expr $checksum + 1`
fi
if test -n "`cmp tv.bv16.bfe10.raw ${TESTDATADIR}/tv.bv16.bfe10.ref.raw`"
then
checksum=`expr $checksum + 1`
fi

if test $checksum -eq 0
then
echo "  **************************************************************************"
echo "  * CONGRATULATIONS: Your compilation passed the simple functionality test *"
echo "  **************************************************************************"
echo ""
\rm tv.bv16 tv.bv16.raw tv.bv16.bfe10.raw
else
echo "  ************************************************************************"
echo "  * WARNING: Your compilation DID NOT pass the simple functionality test *"
echo "  ************************************************************************"
echo ""
fi
