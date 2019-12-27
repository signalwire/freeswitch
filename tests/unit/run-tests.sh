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
	if ls /cores/core.*.!drone!src!tests!unit!.libs!$i.* 1> /dev/null 2>&1; then
	    echo "Coredump found";
	    COREDUMP=$(ls /cores/core.*.!drone!src!tests!unit!.libs!$i.*) ;
	    echo $COREDUMP;
	    gdb -ex "set logging file backtrace_$i.txt" -ex "set logging on" -ex "set pagination off" -ex "bt" -ex "bt full" -ex "info threads" -ex "thread apply all bt" -ex "thread apply all bt full" -ex "quit" /drone/src/tests/unit/.libs/$i $COREDUMP ;
	fi ;
	echo "*** $logfilename was saved" ;
    fi ;
    echo "----------------" ;
done
