#!/bin/bash
INSTALLPREFIX="/usr/local/freetdm"
VERSION=""
NODOCS="NO"
 
for i in $*
do
	case $i in
	--version=*)
	VERSION=`echo $i | sed 's/[-a-zA-Z0-9]*=//'`
	;;
	--prefix=*)
	INSTALLPREFIX=`echo $i | sed 's/[-a-zA-Z0-9]*=//'`
	;;
	--nodocs)
	NODOCS="YES"
	;;
	*)
	# unknown option
	echo "Unknown option $i"
	exit
	;;
	esac
done

if [ "x$VERSION" = "x" ]
then
	echo "Provide a version number with --version=<version>"
	exit 1
fi

arch=$(uname -m)

# defs
LIBSNG_ISDN_URL=ftp://ftp.sangoma.com/linux/libsng_isdn
LIBSNG_ISDN_NAME=libsng_isdn-7-current
LIBSNG_SS7_URL=ftp://ftp.sangoma.com/linux/libsng_ss7
LIBSNG_SS7_NAME=libsng_ss7-3-current
LIBSNG_ISDN_DIR="$LIBSNG_ISDN_NAME.$arch"
LIBSNG_SS7_DIR="$LIBSNG_SS7_NAME.$arch"

# download and decompress a tarball
# $1 = prefix_url, such as ftp://ftp.sangoma.com/foo/bar
# $2 = package name, such as libsng_isdn-7.0.0.x86_64
function download() {
    wget $1/$2.tgz
    if [ $? = 0 ]
    then
        tardir=$(tar -tf $2.tgz | head -n1 | sed 's,\/,,g')
        tar -xvzf $2.tgz || echo "FAILED to decompress $2.tgz"
        if [ "$tardir" != "$2" ]
        then
            mv $tardir $2 || echo "FAILED to move $tardir to $2"
        fi
        echo "SUCCESSFULLY downloaded $2"
    else
        echo "FAILED to download $1/$2.tgz"
    fi
}

# download and build libsng-ss7
fullname="$LIBSNG_ISDN_NAME.$arch"
if [ -d $fullname ]
then
    echo "skipping isdn download since $fullname directory already exists ... remove if you want this step to be performed"
else
    download $LIBSNG_ISDN_URL $fullname
fi

cd $LIBSNG_ISDN_DIR
make DESTDIR=$INSTALLPREFIX install || echo "Failed to build libsng-isdn"
cd ..

# download and build libsng-ss7
fullname="$LIBSNG_SS7_NAME.$arch"
if [ -d $fullname ]
then
    echo "skipping ss7 download since $fullname directory already exists ... remove if you want this step to be performed"
else
    download $LIBSNG_SS7_URL $fullname
fi

cd $LIBSNG_SS7_DIR
make DESTDIR=$INSTALLPREFIX install || echo "Failed to build libsng-ss7"
cd ..

if [ ! -d $INSTALLPREFIX ]
then
	mkdir -p $INSTALLPREFIX || exit 1
fi

if [ ! -d $INSTALLPREFIX/bin-releases ]
then
	mkdir -p $INSTALLPREFIX/bin-releases || exit 1
fi

# attempt to compile freetdm
echo "Build freetdm and mod_freetdm now..."
make all mod_freetdm || exit 1
echo "freetdm built OK"

major=$(echo "$VERSION" | cut -d. -f1)
minor=$(echo "$VERSION" | cut -d. -f2)
micro=$(echo "$VERSION" | cut -d. -f3)
release="freetdm-$VERSION"

# ABI compatibility check
if [ -x /usr/local/bin/ftdm_abi_check.py ]; then
	/usr/local/bin/ftdm_abi_check.py --release_path=$(pwd) --archive_path=$INSTALLPREFIX/bin-releases --version=$VERSION

	if [ $? -ne 0 ]; then
		echo "ABI compabitility test failed, not creating release. Either increment the major version number or fix the interface."
		exit 1
	fi
else
	echo -ne "\n\nWARNING: /usr/local/bin/ftdm_abi_check.py not found, skipping ABI compatibility test\n\n"
fi

if [ $NODOCS = "NO" ]
then
	make dox || exit 1
fi

echo "Creating $release ($major.$minor.$micro) at $INSTALLPREFIX/$release (directory will be removed if exists already) ... "

mkdir -p $INSTALLPREFIX/$release $INSTALLPREFIX/bin-releases/$major/$release

cp -r ./* $INSTALLPREFIX/bin-releases/$major/$release
cp -r ./.libs $INSTALLPREFIX/bin-releases/$major/$release

make clean
make mod_freetdm-clean

cp -r ./* $INSTALLPREFIX/$release

# copy ABI compatibility reports to release
if [ -d compat_reports ]; then
	mv ./compat_reports $INSTALLPREFIX/$release
fi

rm -rf $INSTALLPREFIX/$release/{$LIBSNG_ISDN_DIR,$LIBSNG_SS7_DIR,*.tgz}
rm -rf $INSTALLPREFIX/bin-releases/$major/$release/{$LIBSNG_ISDN_DIR,$LIBSNG_SS7_DIR,*.tgz}

tar -C $INSTALLPREFIX -czf $INSTALLPREFIX/$release.tar.gz $release/
