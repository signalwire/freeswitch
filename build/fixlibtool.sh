cd /tmp
mkdir fix_libtool
cd fix_libtool
wget http://mirrors.kernel.org/gnu/libtool/libtool-1.5.22.tar.gz
tar -zxvf libtool-1.5.22.tar.gz
cd libtool-1.5.22
./configure --prefix=/usr
make
make install
cd /tmp
rm -fr fix_libtool
