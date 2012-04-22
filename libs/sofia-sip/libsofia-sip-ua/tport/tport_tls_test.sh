#!/bin/sh

echo Starting TLS Test Server
./tport_test_server &
sleep 1
./tport_test_client 
