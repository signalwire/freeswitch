#!/usr/bin/env bash

### shfmt -w -s -ci -sr -kp -fn tests/unit/run-tests.sh

#------------------------------------------------------------------------------
# Test Execution Script
# Executes unit tests and handles test logs and core dumps
#------------------------------------------------------------------------------

# Initialize timing
start_time=$(date +%s)

# All output will be collected here
TESTSUNITPATH=$PWD

# All relative paths are based on the tree's root
FSBASEDIR=$(realpath "$PWD/../../")

# Set system limits
ulimit -c unlimited
ulimit -a

# Get test identifier from argument
i=$1

echo ""
echo "Starting test: $i"
echo ""

# Change folder to where the test is
currenttestpath="$FSBASEDIR/$i"
cd "$(dirname "$currenttestpath")"

# Tests are unique per module, so need to distinguish them by their directory
relativedir=$(dirname "$i")
echo "Relative dir is $relativedir"

# Set up log file path
file=$(basename -- "$currenttestpath")
log="$TESTSUNITPATH/log_run-tests_${relativedir//\//!}!$file.html"

# Execute the test
echo "Start executing $currenttestpath"
$currenttestpath 2>&1 | tee >(ansi2html > "$log")
exitstatus=${PIPESTATUS[0]}
echo "End executing $currenttestpath"

# Record execution time
end_time=$(date +%s)
duration=$((end_time - start_time))
echo "Test $1 took $duration seconds" >> test_times.log
echo "Exit status is $exitstatus"

# Handle test results
if [ "0" -eq "$exitstatus" ]; then
	rm "$log"
else
	echo "*** ./$i exit status is $exitstatus"

	# Search for core dumps
	corefilesearch="/cores/core.*.!__w!freeswitch!freeswitch!${relativedir//\//!}!.libs!$file.*"
	echo "$corefilesearch"

	if ls $corefilesearch 1> /dev/null 2>&1; then
		echo "coredump found"
		coredump=$(ls $corefilesearch)
		echo "$coredump"

		# Generate backtrace using GDB
		gdb \
			-ex "set logging file $TESTSUNITPATH/backtrace_${i//\//!}.txt" \
			-ex "set logging on" \
			-ex "set pagination off" \
			-ex "bt full" \
			-ex "bt" \
			-ex "info threads" \
			-ex "thread apply all bt" \
			-ex "thread apply all bt full" \
			-ex "quit" \
			"$FSBASEDIR/$relativedir/.libs/$file" \
			"$coredump"
	fi

	echo "*** $log was saved"

	exit $exitstatus
fi

echo ""
