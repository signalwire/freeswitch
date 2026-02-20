#!/usr/bin/env bash

### shfmt -w -s -ci -sr -kp -fn tests/unit/collect-test-logs.sh

#------------------------------------------------------------------------------
# Collects test logs and generates an HTML report
# of failed unit tests with links to backtraces
#------------------------------------------------------------------------------

# Configuration with default value
LOG_DIR="./logs"
PRINT_TO_CONSOLE=0

# Parse command line arguments
while [[ $# -gt 0 ]]; do
	case $1 in
		-p | --print)
			if ! command -v html2text > /dev/null 2>&1; then
				echo "Error: html2text is required for printing HTML contents"
				echo "Please install html2text and try again"
				exit 1
			fi
			PRINT_TO_CONSOLE=1
			shift
			;;
		-d | --dir)
			if [ -z "$2" ]; then
				echo "Error: Log directory path is required for -d|--dir option"
				exit 1
			fi
			LOG_DIR="$2"
			shift 2
			;;
		*)
			echo "Unknown option: $1"
			echo "Usage: $0 [-p|--print] [-d|--dir DIR]"
			exit 1
			;;
	esac
done

# Initialize HTML
echo "Collecting test logs"
html="<html><head><title>Failed Unit Tests Report</title></head><body><h3>There are failed unit-tests:</h3><table>"

# Find all HTML log files and sort them
logs=$(find "$LOG_DIR" -type f -iname "*.html" -print | sort)
logs_found=0
olddirname=""

# Process each log file
for name in $logs; do
	# Extract test information
	logname=$(basename "$name")
	testname=$(echo "$logname" | awk -F 'log_run-tests_' '{print $2}' | awk -F '.html' '{print $1}')
	testpath="${testname//!/\/}"
	dirname=$(dirname "$testpath")
	test=$(basename "$testpath")

	# Add directory header if it's new
	if [ "$olddirname" != "$dirname" ]; then
		html+="<tr align=\"left\"><th><br>$dirname</th></tr>"
		olddirname=$dirname
	fi

	# Add test entry
	html+="<tr align=\"left\"><td><a href=\"$logname\">$test</a>"

	# Check for backtrace
	backtrace="backtrace_$testname.txt"
	if test -f "${LOG_DIR}/$backtrace"; then
		if [ $PRINT_TO_CONSOLE -eq 1 ]; then
			echo "Core dumped, backtrace:"
			cat $backtrace
			echo
		fi

		html+=". Core dumped, backtrace is available <a href=\"$backtrace\">here</a>"
	fi

	html+="</td></tr>"
	logs_found=1

	# Print current log file if requested
	if [ $PRINT_TO_CONSOLE -eq 1 ]; then
		echo "=== Contents of $name ==="
		html2text "$name"
		echo "=== End of $name ==="
		echo
	fi
done

# Generate report if logs were found
if [ $logs_found -ne 0 ]; then
	html+="</table></body></html>"
	echo "$html" > "$LOG_DIR/artifacts.html"
	exit 1
fi

exit 0
