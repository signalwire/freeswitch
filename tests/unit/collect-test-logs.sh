#!/bin/bash

echo "Collecting test logs"
LOG_DIR=./logs
html="<html><h3>There are failed unit-tests:</h3><table>"
logs=$(find $LOG_DIR -type f -iname "*.html" -print | sort)
logs_found=0
olddirname=""
for name in $logs
do
	logname=$(basename $name)
	testname=$(echo $logname | awk -F 'log_run-tests_' '{print $2}' | awk -F '.html' '{print $1}')
	testpath="${testname//!/\/}"
	dirname=$(dirname $testpath)
	test=$(basename $testpath)
	if [ "$olddirname" != "$dirname" ]; then
		html+="<tr align=\"left\"><th><br>$dirname</th></tr>" ;
		olddirname=$dirname ;
	fi
	html+="<tr align=\"left\"><td><a href="$logname">$test</a>"
	backtrace="backtrace_$testname.txt"
	if test -f "${LOG_DIR}/$backtrace"; then
		html+=". Core dumped, backtrace is available <a href=\"$backtrace\">here</a>"
	fi
	html+="</td></tr>"
	logs_found=1
done

if [ $logs_found -ne 0 ]; then
	html+="</table></html>"
	echo $html > $LOG_DIR/artifacts.html
	exit 1
fi

exit 0
