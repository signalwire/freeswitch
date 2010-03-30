#!/bin/sh

errorexit() {
    echo $1
    exit 1
}

# run the test
./testparser > testparser.log

# check if output differs from what it should be
diff testparser.log testparser-output.txt
