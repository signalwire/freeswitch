#!/bin/sh

errorexit() {
    echo $1
    exit 1
}

cp spb.pb spb-copy.pb || errorexit "could not copy spb.pb to spb-copy.pb"

# run the test
./testspb > testspb.log

# add new contents of phonebook file to the test log
cat spb-copy.pb >> testspb.log

# check if output differs from what it should be
diff testspb.log testspb-output.txt
