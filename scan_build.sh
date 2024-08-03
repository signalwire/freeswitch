#!/bin/bash
mkdir -p scan-build
scan-build-14 --force-analyze-debug-code -o ./scan-build/ make -j`nproc --all` |& tee ./scan-build-result.txt
exitstatus=${PIPESTATUS[0]}
echo "*** Exit status is $exitstatus"
export SubString="scan-build: No bugs found"
export COMPILATION_FAILED="false"
export BUGS_FOUND="false"
if [[ "0" != "$exitstatus" ]] ; then
  export COMPILATION_FAILED="true"
  echo MESSAGE="compilation failed" >> $GITHUB_OUTPUT
fi
export RESULTFILE="$PWD/scan-build-result.txt"
# cat $RESULTFILE
if ! grep -sq "$SubString" $RESULTFILE; then
  export BUGS_FOUND="true"
  echo MESSAGE="found bugs" >> $GITHUB_OUTPUT
fi
export REPORT=$PWD/`find scan-build* -mindepth 1 -type d`
echo "COMPILATION_FAILED: $COMPILATION_FAILED"
echo "BUGS_FOUND: $BUGS_FOUND"
echo "COMPILATION_FAILED=$COMPILATION_FAILED" >> $GITHUB_OUTPUT
echo "BUGS_FOUND=$BUGS_FOUND" >> $GITHUB_OUTPUT
echo "REPORT=$REPORT" >> $GITHUB_OUTPUT
if [[ "0" != "$exitstatus" ]] || ! grep -sq "$SubString" $RESULTFILE; then
  exit 1
fi
exit 0
