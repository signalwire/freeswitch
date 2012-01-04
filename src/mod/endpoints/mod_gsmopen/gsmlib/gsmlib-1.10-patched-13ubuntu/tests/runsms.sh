#!/bin/sh

errorexit() {
    echo $1
    exit 1
}

# prepare locales to make date format reproducible
LC_ALL=en_US
LANG=en
LINGUAS=en
export LC_ALL LANG LINGUAS

# run the test
./testsms > testsms.log

# check if output differs from what it should be
diff testsms.log testsms-output.txt
