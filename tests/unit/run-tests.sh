#!/bin/bash

# "print_tests" returns relative paths to all the tests
TESTS=$(make -s -C ../.. print_tests)

echo "-----------------------------------------------------------------";
echo "Starting tests";
echo "Tests found: ${TESTS}";
echo "-----------------------------------------------------------------";
echo "Starting" > pids.txt
for i in $TESTS
do
    echo "Testing $i" ;
    ./test.sh "$i" &
    pid=($!)
    pids+=($pid)
    echo "$pid $i" >> pids.txt
    echo "----------------" ;
done

for pid in "${pids[@]}"
do
  echo "$pid waiting" >> pids.txt
  wait "$pid"
  echo "$pid finished" >> pids.txt
done

echo "Done running tests!"