#!/bin/sh

./configure $@
$MAKE depend
cd libraries
$MAKE
$MAKE install
cd ../include
cp -p ldap*.h lber*.h $PREFIX/include
