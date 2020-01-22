#!/bin/sh

apt-get clean && apt-get update && apt-get -y upgrade && apt-get -y dist-upgrade

apt-get install -y wget git

wget -O - https://files.freeswitch.org/repo/deb/debian/freeswitch_archive_g0.pub | apt-key add -

echo "deb http://files.freeswitch.org/repo/deb/freeswitch-1.6/ jessie main" > /etc/apt/sources.list.d/freeswitch.list

apt-get update && apt-get install -y libtool libjpeg62-turbo-dev ntpdate libfreetype6-dev git-buildpackage doxygen yasm gdb git build-essential automake autoconf wget uuid-dev zlib1g-dev libncurses5-dev libssl-dev libpcre3-dev libcurl4-openssl-dev libldns-dev libedit-dev libspeexdsp-dev libsqlite3-dev perl libgdbm-dev libdb-dev bison pkg-config ccache libpng16-dev libpng12-dev libopenal-dev libbroadvoice-dev libcodec2-dev libflite-dev libg7221-dev libilbc-dev libsilk-dev liblua5.2-dev libopus-dev libsndfile-dev libavformat-dev libavcodec-extra libx264-dev libperl-dev unixodbc-dev libpq-dev libsctp-dev

cd /usr/src

git clone https://github.com/signalwire/freeswitch.git freeswitch.git

cd freeswitch.git

# The -j argument spawns multiple threads to speed the build process, but causes trouble on some systems
./bootstrap.sh -j

./configure -C --enable-portable-binary --enable-sctp\
	   --prefix=/usr --localstatedir=/var --sysconfdir=/etc \
	   --with-gnu-ld --with-openssl \
	   --enable-core-odbc-support --enable-zrtp \
	   --enable-core-pgsql-support \
	   --enable-static-v8 --disable-parallel-build-v8 --enable-amr $@

#CC=clang-3.6 CXX=clang++-3.6 ./configure -C --enable-portable-binary \
#           --prefix=/usr --localstatedir=/var --sysconfdir=/etc \
#           --with-gnu-ld --with-openssl \
#           --enable-core-odbc-support --enable-zrtp \
#           --enable-core-pgsql-support \
#           --enable-static-v8 --disable-parallel-build-v8 --enable-address-sanitizer

make

make -j install
