#!/bin/sh

STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

for OPTS in "-g -b 9600 -s -42 -n -62" "-g -b 7200 -s -42 -n -59" "-g -b 4800 -s -42 -n -54"
do
    ./v29_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo v29_tests ${OPTS} failed!
        exit $RETVAL
    fi
done
echo v29_tests completed OK
