#!/bin/bash
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

eval $(parse_version "$1")

dst_name="freeswitch-$ver"
dst_cname="freeswitch-$cmajor.$cminor.$cmicro"
dst_parent="${tmp_dir}/jenkis.$$/"
dst_dir="${tmp_dir}/jenkins.$$/$dst_name"

if [ -d "$dst_dir" ]; then
  echo "error: destination directory $dst_dir already exists." 1>&2
  exit 1;
fi

mkdir -p $dst_dir
cp -r . $dst_dir

cd $dst_dir

sed -e "s|\(AC_SUBST(SWITCH_VERSION_MAJOR, \[\).*\(\])\)|\1$major\2|" \
  -e "s|\(AC_SUBST(SWITCH_VERSION_MINOR, \[\).*\(\])\)|\1$minor\2|" \
  -e "s|\(AC_SUBST(SWITCH_VERSION_MICRO, \[\).*\(\])\)|\1$micro\2|" \
  -e "s|\(AC_INIT(\[freeswitch\], \[\).*\(\], BUG-REPORT-ADDRESS)\)|\1$ver\2|" \
  -i configure.in

if [ -n "$rev" ]; then
  sed -e "s|\(AC_SUBST(SWITCH_VERSION_REVISION, \[\).*\(\])\)|\1$rev\2|" \
    -e "s|#\(AC_SUBST(SWITCH_VERSION_REVISION\)|\1|" \
    -i configure.in
fi

./bootstrap.sh -j
mv bootstrap.sh rebootstrap.sh
rm -f docs/AUTHORS
rm -f docs/COPYING
rm -f docs/ChangeLog
rm -rf .git
cd ..

cd $dst_parent

ls

tar -cvf ${dst_name}.tar $dst_name

# gzip -9 -c ${dst_name}.tar > $dst_name.tar.gz || echo "gzip not available"
bzip2 -z -k ${dst_name}.tar || echo "bzip2 not available"
cp -al ${dst_name}.tar.bz2 ${dst_cname}.tar.bz2
# xz -z -9 -k ${dst_name}.tar || echo "xz / xz-utils not available"

rm -rf ${dst_name}.tar $dst_dir

mkdir -p ${src_repo}/src_dist
mv -f ${dst_name}.tar.* ${src_repo}/src_dist
mv -f ${dst_cname}.tar.* ${src_repo}/src_dist

cat 1>&2 <<EOF
----------------------------------------------------------------------
The freeswitch-${ver} tarballs have been rolled,
now we just need to roll packages with them
----------------------------------------------------------------------
EOF

