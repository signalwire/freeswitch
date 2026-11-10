#!/bin/sh
cd test
cd stir-shaken/www
if command -v python3 >/dev/null 2>&1; then
	python3 -m http.server 8080 &
elif command -v python >/dev/null 2>&1 && python -c 'import http.server' >/dev/null 2>&1; then
	python -m http.server 8080 &
else
	python -m SimpleHTTPServer 8080 &
fi
ppid=$!
cd ../..
./test_sofia_funcs "$@"
test=$?
kill $ppid 2>/dev/null
wait $ppid 2>/dev/null
exit $test
