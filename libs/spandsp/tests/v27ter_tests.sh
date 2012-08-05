#!/bin/sh

STDOUT_DEST=xyzzy
STDERR_DEST=xyzzy2

for OPTS in "-g -b 4800 -s -42 -n -57" "-g -b 2400 -s -42 -n -51"
do
    ./v27ter_tests ${OPTS} >$STDOUT_DEST 2>$STDERR_DEST
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo v27ter_tests ${OPTS} failed!
        exit $RETVAL
    fi
done
echo v27ter_tests completed OK
