#!/bin/sh

MY_CWD=$(pwd)

report_error()
{
    echo "$1"

    # Go back to the start directory and cleanup before we exit
    cd "$MY_CWD"
    rm -rf v8-$VERSION*

    exit 1
}
        
VERSION=$1

if [ "$VERSION" = "" ]; then
    report_error "You must provide the version to build!"
fi

echo "Will build tarballs for V8 version $VERSION"

echo "Cleaning up earlier builds..."
rm -rf v8-$VERSION* || report_error "Failed to cleanup, missing rights?"

# Get current trunk version (which is the latest stable release)
echo "Cloning V8 git repo..."
git clone git://github.com/v8/v8.git v8-$VERSION > /dev/null || report_error "Failed to git clone Google V8"
cd v8-$VERSION > /dev/null || report_error "Failed to enter the cloned directory"
echo "Checking out version $VERSION from local V8 repo"
git checkout tags/$VERSION > /dev/null || report_error "Failed to get version $VERSION"

# Download dependencies releated to this version
echo "Downloading V8 dependencies"
make dependencies > /dev/null || report_error "Failed to get V8 dependencies"

# Cleanup files we don't need
echo "Cleaning up .git and .svn directories"
for f in $(find | grep "\.git$")
do
    rm -rf "$f"
done

for f in $(find | grep "\.gitignore$")
do
    rm -rf "$f"
done

for f in $(find | grep "\.svn$")
do
    rm -rf "$f"
done

echo "Removing tests from build and packaging"
rm -rf test
rm -rf third_party/icu/source/test
patch -p1 < ../remove-v8-tests.patch || report_error "Failed to apply build patch!"

cd .. || report_error "Failed to change dir"

echo "Creating new tarball..."
tar cjpf v8-$VERSION.tar.bz2 v8-$VERSION || report_error "Failed to create tarball!"

echo "cleaning up temporary data.."
rm -rf v8-$VERSION || report_error "Failed to cleanup"

echo "Finished building v8-$VERSION.tar.bz2"

echo "Start building v8-$VERSION-win.tar.bz2"

mkdir v8-$VERSION || report_error "Failed to create directory"
cd v8-$VERSION || report_error "Failed to change dir"
mkdir third_party || report_error "Failed to create directory"
cd third_party || report_error "Failed to change dir"

echo "Checking out files for Windows build..."
svn co http://src.chromium.org/svn/trunk/tools/third_party/python_26 python_26 --revision 89111 > /dev/null || report_error "Failed to checkout python files"
svn co http://src.chromium.org/svn/trunk/deps/third_party/cygwin cygwin --revision 231940 > /dev/null || report_error "Failed to checkout cygwin files"

echo "Cleaning up .svn directories..."
for f in $(find | grep "\.svn$")
do
    rm -rf "$f"
done

cd ../.. || report_error "Failed to change dir"

echo "Creating new tarball..."
tar cjpf v8-$VERSION-win.tar.bz2 v8-$VERSION || report_error "Failed to create tarball"

echo "Cleaning up..."
rm -rf v8-$VERSION
