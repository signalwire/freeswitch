#!/bin/bash

TESTS=$(make -f -  2>/dev/null <<EOF
include Makefile
all:
	@echo \$(TESTS)
EOF
)

echo "-----------------------------------------------------------------";
echo "Starting tests";
echo "Tests found: ${TESTS}";
echo "-----------------------------------------------------------------";
for i in $TESTS
do
    echo "Testing $i" ;
    logfilename="log_run-tests_$i.html";
    ./$i | tee >(ansi2html > $logfilename) ;
    exitstatus=${PIPESTATUS[0]} ;
    if [ "0" -eq $exitstatus ] ; then
	rm $logfilename ;
    else
	echo "*** ./$i exit status is $exitstatus" ;
	echo "*** $logfilename was saved" ;
    fi ;
    echo "----------------" ;
done
