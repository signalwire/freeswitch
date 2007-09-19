cd /tmp
mkdir fix_automake
cd fix_automake
wget http://mirrors.kernel.org/gnu/automake/automake-1.10.tar.gz
tar -zxvf automake-1.10.tar.gz
cd automake-1.10
./configure --prefix=/usr
make
make install
cd /tmp
rm -fr fix_automake

