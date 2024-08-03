#!/bin/bash

# "print_tests" returns relative paths to all the tests
TESTS=$(make -s -C ../.. print_tests)

chunks=${1:-1}
chunk_number=${2:-1}

IFS=$'\n' read -d '' -r -a lines <<< "$TESTS"

result=""
for ((i=chunk_number-1; i<${#lines[@]}; i+=chunks))
do
  result+="${lines[$i]}"$'\n'
done

TESTS=$result

echo "-----------------------------------------------------------------";
echo "Starting tests on $(nproc --all) processors";
echo "Tests found: ${TESTS}";
echo "-----------------------------------------------------------------";

make -f run-tests.mk TEST_LIST=$TESTS

echo "Timing results:"
cat test_times.log

echo "Done running tests!"
