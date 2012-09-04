#!/bin/sh

STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

for OPTS in "-b 14400 -s -42 -n -66" "-b 12000 -s -42 -n -61" "-b 9600 -s -42 -n -59" "-b 7200 -s -42 -n -56"
do
    ./v17_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo v17_tests ${OPTS} failed!
        exit $RETVAL
    fi
done
echo v17_tests completed OK
