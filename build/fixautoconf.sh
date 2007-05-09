cd /tmp
mkdir fix_autoconf
cd fix_autoconf
wget http://mirrors.kernel.org/gnu/autoconf/autoconf-2.61.tar.gz
tar -zxvf autoconf-2.61.tar.gz
cd autoconf-2.61
./configure --prefix=/usr
make
make install
cd /tmp
rm -fr fix_autoconf

