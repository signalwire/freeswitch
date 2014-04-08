#!/bin/sh
# 
# usage: rtpw_test <rtpw_commands>
# 
# tests the rtpw sender and receiver functions

RTPW=./rtpw
DEST_PORT=9999
DURATION=3

# First, we run "killall" to get rid of all existing rtpw processes.
# This step also enables this script to clean up after itself; if this
# script is interrupted after the rtpw processes are started but before
# they are killed, those processes will linger.  Re-running the script
# will get rid of them.

killall rtpw 2>/dev/null

if test -x $RTPW; then

GCMARGS128="-k 01234567890123456789012345678901234567890123456789012345 -g -e 128"
echo  $0 ": starting GCM mode 128-bit rtpw receiver process... "

exec $RTPW $* $GCMARGS128 -r 127.0.0.1 $DEST_PORT &

receiver_pid=$!

echo $0 ": receiver PID = $receiver_pid"

sleep 1 

# verify that the background job is running
ps | grep -q $receiver_pid
retval=$?
echo $retval
if [ $retval != 0 ]; then
    echo $0 ": error"
    exit 254
fi

echo  $0 ": starting GCM 128-bit rtpw sender process..."

exec $RTPW $* $GCMARGS128 -s 127.0.0.1 $DEST_PORT  &

sender_pid=$!

echo $0 ": sender PID = $sender_pid"

# verify that the background job is running
ps | grep -q $sender_pid
retval=$?
echo $retval
if [ $retval != 0 ]; then
    echo $0 ": error"
    exit 255
fi

sleep $DURATION

kill $receiver_pid
kill $sender_pid



GCMARGS256="-k 0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567 -g -e 256"
echo  $0 ": starting GCM mode 256-bit rtpw receiver process... "

exec $RTPW $* $GCMARGS256 -r 127.0.0.1 $DEST_PORT &

receiver_pid=$!

echo $0 ": receiver PID = $receiver_pid"

sleep 1 

# verify that the background job is running
ps | grep -q $receiver_pid
retval=$?
echo $retval
if [ $retval != 0 ]; then
    echo $0 ": error"
    exit 254
fi

echo  $0 ": starting GCM 256-bit rtpw sender process..."

exec $RTPW $* $GCMARGS256 -s 127.0.0.1 $DEST_PORT  &

sender_pid=$!

echo $0 ": sender PID = $sender_pid"

# verify that the background job is running
ps | grep -q $sender_pid
retval=$?
echo $retval
if [ $retval != 0 ]; then
    echo $0 ": error"
    exit 255
fi

sleep $DURATION

kill $receiver_pid
kill $sender_pid


echo $0 ": done (test passed)"

else 

echo "error: can't find executable" $RTPW
exit 1

fi

# EOF


