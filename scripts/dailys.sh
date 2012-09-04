#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

src_repo="$(pwd)"

if [ ! -d .git ]; then
  echo "error: must be run from within the top level of a FreeSWITCH git tree." 1>&2
  exit 1;
fi

if [ -z "$1" ]; then
  echo "usage: ./scripts/dailys.sh MAJOR.MINOR.MICRO[.REVISION] BUILD_NUMBER" 1>&2
  exit 1;
fi

ver="$1"
major=$(echo "$ver" | cut -d. -f1)
minor=$(echo "$ver" | cut -d. -f2)
micro=$(echo "$ver" | cut -d. -f3)
rev=$(echo "$ver" | cut -d. -f4)

build="$2"

dst_name="freeswitch-$major.$minor.$micro"
dst_parent="/tmp/"
dst_dir="/tmp/$dst_name"

if [ -d "$dst_dir" ]; then
  echo "error: destination directory $dst_dir already exists." 1>&2
  exit 1;
fi

# save local changes
#ret=$(git stash save "Save uncommitted changes before tagging.")
#if echo $ret | grep "^Saved"; then
    #stash_saved=1
#fi

sed -e "s|\(AC_SUBST(SWITCH_VERSION_MAJOR, \[\).*\(\])\)|\1$major\2|" \
  -e "s|\(AC_SUBST(SWITCH_VERSION_MINOR, \[\).*\(\])\)|\1$minor\2|" \
  -e "s|\(AC_SUBST(SWITCH_VERSION_MICRO, \[\).*\(\])\)|\1$micro\2|" \
  -e "s|\(AC_INIT(\[freeswitch\], \[\).*\(\], BUG-REPORT-ADDRESS)\)|\1$major.$minor.$micro\2|" \
  -i configure.in

if [ -n "$rev" ]; then
  sed -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION, \[\).*\(\])\)|\1$rev\2|" \
    -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION_HUMAN, \[\).*\(\])\)|\1$rev\2|" \
    -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION\)|\1|" \
    -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION_HUMAN\)|\1|" \
    -i configure.in
fi

#git add configure.in
#git commit -m "Release freeswitch-$ver"
#git tag -a -m "freeswitch-$ver release" v$ver

#git clone $src_repo $dst_dir
#if [ -n "$stash_saved" ]; then
#    git stash pop
#fi

cp -r . $dst_dir

cd $dst_dir

./bootstrap.sh
mv bootstrap.sh rebootstrap.sh
rm -f docs/AUTHORS
rm -f docs/COPYING
rm -f docs/ChangeLog
rm -rf .git
cd ..
cd $dst_parent
tar -cvf $dst_name.tar $dst_name

# gzip -9 -c $dst_name.tar > $dst_name.tar.gz || echo "gzip not available"
bzip2 -z -k $dst_name.tar || echo "bzip2 not available"
# xz -z -9 -k $dst_name.tar || echo "xz / xz-utils not available"
rm -rf $dst_name.tar $dst_dir

mkdir -p $src_repo/rpmbuild/{SOURCES,BUILD,BUILDROOT,i386,x86_64,SOURCES,SPECS}
mv -f $dst_name.tar.* $src_repo/rpmbuild/SOURCES/.

cd $src_repo/rpmbuild/SOURCES

for i in `grep 'Source..\?:' $src_repo/freeswitch.spec|grep -v Source0 |awk '{print $2}'`; do wget $i; done

cd $src_repo

rpmbuild --define "VERSION_NUMBER $ver" \
  --define "BUILD_NUMBER $build" \
  --define "_topdir %(pwd)/rpmbuild" \
  --define "_rpmdir %{_topdir}" \
  --define "_srcrpmdir %{_topdir}" \
  -ba freeswitch.spec

# --define '_rpmfilename %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm' \
# --define "_sourcedir  %{_topdir}" \
# --define "_builddir %{_topdir}" \


mkdir $src_repo/RPMS
mv $src_repo/rpmbuild/*/*.rpm $src_repo/RPMS/.

cat 1>&2 <<EOF
----------------------------------------------------------------------
The v$ver-$build tarballs and RPMs have been rolled, now we
just need to roll packages with them
----------------------------------------------------------------------
EOF

