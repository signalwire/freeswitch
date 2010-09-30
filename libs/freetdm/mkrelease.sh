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

if [ ! -d $INSTALLPREFIX ]
then
	mkdir -p $INSTALLPREFIX || exit 1
fi

make clean
make mod_freetdm-clean
if [ $NODOCS = "NO" ]
then
	make dox || exit 1
fi

major=$(echo "$VERSION" | cut -d. -f1)
minor=$(echo "$VERSION" | cut -d. -f2)
micro=$(echo "$VERSION" | cut -d. -f3)
release="freetdm-$VERSION"

echo "Creating $release ($major.$minor.$micro) at $INSTALLPREFIX/$release (directory will be removed if exists already) ... press any key to continue"
read

mkdir -p $INSTALLPREFIX/$release

cp -r ./* $INSTALLPREFIX/$release

find $INSTALLPREFIX/ -name .libs -exec rm -rf {} \;
find $INSTALLPREFIX/ -name .deps -exec rm -rf {} \;
find $INSTALLPREFIX/ -name *.so -exec rm -rf {} \;
find $INSTALLPREFIX/ -name *.lo -exec rm -rf {} \;


tar -C $INSTALLPREFIX -czf $INSTALLPREFIX/$release.tar.gz $release/


