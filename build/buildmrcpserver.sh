#!/bin/sh
base=`pwd`
cd libs/unimrcp
./configure --with-pocketsphinx=$base/libs/pocketsphinx-0.5.99 --with-sphinxbase=$base/libs/sphinxbase-0.4.99 --with-flite=$base/libs/flite-1.3.99 --with-apr=$base/libs/apr --with-apr-util=$base/libs/apr-util --with-sofia-sip=$base/libs/sofia-sip --prefix=/usr/local/unimrcpserver --enable-pocketsphinx-plugin --enable-flite-plugin --disable-demosynth-plugin --disable-demorecog-plugin --disable-recorder-plugin --disable-cepstral-plugin
make
make install
