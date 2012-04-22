#!/bin/sh

errorexit() {
    echo $1
    exit 1
}

rm -f sms.sms || errorexit "could not delete sms.sms"
touch sms.sms || errorexit "could not create sms.sms"

# prepare locales to make date format reproducible
LC_ALL=en_US
LANG=en
LINGUAS=en
export LC_ALL LANG LINGUAS

# run the test
./testssms > testssms.log

# check if output differs from what it should be
diff testssms.log testssms-output.txt
