#!/bin/sh

# This script is used so the thisbuild compile project can have multiple
# architectures compile the software to check portability. Please leave
# this file in tact! - Justin

../build.sh build 2>&1
ojnk=$?

echo "Return Code: $ojnk"

