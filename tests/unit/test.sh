#!/bin/bash

# All output will be collected here
TESTSUNITPATH=$PWD

# All relative paths are based on the tree's root
FSBASEDIR=$(realpath "$PWD/../../")

i=$1

echo "----------------------------------" ;
echo "Starting test: $i" ;
echo "----------------------------------" ;

# Change folder to where the test is
currenttestpath="$FSBASEDIR/$i"
cd $(dirname "$currenttestpath")

# Tests are unique per module, so need to distinguish them by their directory
relativedir=$(dirname "$i")
echo "Relative dir is $relativedir"

file=$(basename -- "$currenttestpath")
log="$TESTSUNITPATH/log_run-tests_${relativedir//\//!}!$file.html";

# Execute the test
echo "Start executing $currenttestpath"
$currenttestpath 2>&1 | tee >(ansi2html > $log) ;
exitstatus=${PIPESTATUS[0]} ;
echo "End executing $currenttestpath"
echo "Exit status is $exitstatus"

if [ "0" -eq $exitstatus ] ; then
	rm $log ;
else
	echo "*** ./$i exit status is $exitstatus" ;
	corefilesearch=/cores/core.*.!drone!src!${relativedir//\//!}!.libs!$file.* ;
	echo $corefilesearch ;
	if ls $corefilesearch 1> /dev/null 2>&1; then
	    echo "coredump found";
	    coredump=$(ls $corefilesearch) ;
	    echo $coredump;
	    echo "set logging file $TESTSUNITPATH/backtrace_${i//\//!}.txt" ;
	    gdb -ex "set logging file $TESTSUNITPATH/backtrace_${i//\//!}.txt" -ex "set logging on" -ex "set pagination off" -ex "bt full" -ex "bt" -ex "info threads" -ex "thread apply all bt" -ex "thread apply all bt full" -ex "quit" /drone/src/$relativedir/.libs/$file $coredump ;
	fi ;
	echo "*** $log was saved" ;
fi ;
echo "----------------" ;
