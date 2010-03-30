#!/bin/sh

errorexit() {
    echo $1
    exit 1
}

cp spb2.pb spb-copy.pb || errorexit "could not copy spb2.pb to spb2-copy.pb"

# run the test
./testspb > testspb2.log

# add new contents of phonebook file to the test log
cat spb-copy.pb >> testspb2.log

# check if output differs from what it should be
diff testspb2.log testspb2-output.txt
