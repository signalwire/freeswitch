#!/bin/sh
FS_DIR=`pwd`
cd /root
svn co https://opalvoip.svn.sourceforge.net/svnroot/opalvoip/ptlib/trunk ptlib
cd ptlib
./configure --prefix=/usr
make
make install
cd ..
svn co https://opalvoip.svn.sourceforge.net/svnroot/opalvoip/opal/branches/v3_6 opal 
cd opal
export PKG_CONFIG_PATH=/usr/lib/pkgconfig 
./configure --prefix=/usr
make
make install
cd ${FS_DIR}
make mod_opal-install
