#!/bin/bash

echo "Collecting test logs"
LOG_DIR=./logs
html="<html><h3>There are failed unit-tests:</h3><table>"
html+="<tr align=\"left\"><th><br>Unit tests</th></tr>"
logs=$(find $LOG_DIR -type f -iname "*.html" -print)
logs_found=0
for name in $logs
do
	logname=$(basename $name)
	testname=$(echo $logname | awk -F 'log_run-tests_' '{print $2}' | awk -F '.html' '{print $1}')
	html+="<tr align=\"left\"><td><a href="$logname">$testname</a></td></tr>"
	logs_found=1
done

if [ $logs_found -ne 0 ]; then
	html+="</table></html>"
	echo $html > $LOG_DIR/artifacts.html
	exit 1
fi

exit 0
