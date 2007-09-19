cd /tmp
mkdir fix_libtool
cd fix_libtool
wget http://mirrors.kernel.org/gnu/libtool/libtool-1.5.24.tar.gz
tar -zxvf libtool-1.5.24.tar.gz
cd libtool-1.5.24
if test "`uname -s`" = "Darwin"; then
./configure --prefix=/usr --program-prefix=g
else
./configure --prefix=/usr
fi
make
make install
cd /tmp
rm -fr fix_libtool
