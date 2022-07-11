#/bin/sh
cd test
pushd stir-shaken/www
python -m SimpleHTTPServer 8080 &
ppid=$!
popd
./test_sofia_funcs $@
test=$?
kill $ppid
wait $ppid
exit $test
