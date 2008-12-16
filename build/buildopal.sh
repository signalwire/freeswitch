#!/bin/sh
cd /root
svn co https://opalvoip.svn.sourceforge.net/svnroot/opalvoip/ptlib/trunk ptlib
cd ptlib
./configure --prefix=/usr
make
make install
cd ..
svn co https://opalvoip.svn.sourceforge.net/svnroot/opalvoip/opal/trunk opal
cd opal
PKG_CONFIG_PATH=/usr/lib/pkgconfig ./configure --prefix=/usr
make
make install
