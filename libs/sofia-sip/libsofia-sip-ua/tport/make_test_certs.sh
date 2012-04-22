#!/bin/sh

basedir=`dirname $0`

mkdir -p test/server
mkdir -p test/client

./tport_rand tls_seed.dat

cp tls_seed.dat test/server
cp tls_seed.dat test/client

./make_root_cert.pl -cn test_root -dns test_root.sip.nokia.com
cp rootcert.pem test/client/cafile.pem
cp rootcert.pem test/server/cafile.pem

./make_node_cert.pl -cn test_client -dns test_client.sip.nokia.com
cp agent.pem test/client

./make_node_cert.pl -cn test_server -dns test_server.sip.nokia.com
cp agent.pem test/server

rm agent*.pem
rm root*.pem
rm tls_seed.dat

