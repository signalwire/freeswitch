#!/bin/bash

src_repo="$(pwd)"

if [ ! -d .git ]; then
    echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
    exit 1;
fi

if [ -z "$1" ]; then
    echo "usage: ./scripts/tagscript.sh MAJOR.MINOR.MICRO[.REVISION]" 1>&2
    exit 1;
fi

ver="$1"
major=$(echo "$ver" | cut -d. -f1)
minor=$(echo "$ver" | cut -d. -f2)
micro=$(echo "$ver" | cut -d. -f3)
rev=$(echo "$ver" | cut -d. -f4)

dst_name="freeswitch-$major.$minor.$micro"
dst_dir="$src_repo/../$dst_name"

if [ -d "$dst_dir" ]; then
    echo "error: destination directory $dst_dir already exists." 1>&2
    exit 1;
fi

# save local changes
ret=$(git stash save "Save uncommitted changes before tagging.")
if echo $ret | grep "^Saved"; then
    stash_saved=1
fi

sed -e "s|\(AC_SUBST(SWITCH_VERSION_MAJOR, \[\).*\(\])\)|\1$major\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MINOR, \[\).*\(\])\)|\1$minor\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_MICRO, \[\).*\(\])\)|\1$micro\2|" \
    -e "s|\(AC_INIT(\[freeswitch\], \[\).*\(\], BUG-REPORT-ADDRESS)\)|\1$major.$minor.$micro\2|" \
    -i configure.in

if [ -n "$rev" ]; then
    sed -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION, \[\).*\(\])\)|\1$rev\2|" \
        -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION\)|\1|" \
        -i configure.in
fi

git add configure.in
git commit -m "Release freeswitch-$ver"
git tag -a -m "freeswitch-$ver release" v$ver

git clone $src_repo $dst_dir
if [ -n "$stash_saved" ]; then
    git stash pop
fi

cd $dst_dir

./bootstrap.sh
mv bootstrap.sh rebootstrap.sh
rm -f docs/AUTHORS
rm -f docs/COPYING
rm -f docs/ChangeLog
rm -rf .git
cd ..
tar -cvf $dst_name.tar $dst_dir
gzip -9 -c $dst_name.tar > $dst_name.tar.gz || echo "gzip not available"
bzip2 -z -k $dst_name.tar || echo "bzip2 not available"
xz -z -9 -k $dst_name.tar || echo "xz / xz-utils not available"
rm -rf $dst_name.tar $dst_dir

cat 1>&2 <<EOF
----------------------------------------------------------------------
The v$ver tag has been committed locally, but it will not be
globally visible until you 'git push --tags' this repository up to the
server (I didn't do that for you, as you might want to review first).
----------------------------------------------------------------------
EOF

