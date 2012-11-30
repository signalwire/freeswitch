#!/bin/sh
##### -*- mode:shell-script; indent-tabs-mode:nil; sh-basic-offset:2 -*-

sdir="."
[ -n "${0%/*}" ] && sdir="${0%/*}"
. $sdir/common.sh

check_pwd
check_input_ver_build $@
eval $(parse_version "$1")
if [ -n "$grev" ]; then 
	dst_name="freeswitch-$cmajor.$cminor.$cmicro.$grev"
else
	dst_name="freeswitch-$cmajor.$cminor.$cmicro"
fi

dst_dir="${tmp_dir}/jenkins.$$/$dst_name"

if [ -d "$dst_dir" ]; then
  echo "error: destination directory $dst_dir already exists." 1>&2
  exit 1;
fi

mkdir -p $dst_dir
cp -r . $dst_dir

cd $dst_dir
set_fs_ver "$gver" "$gmajor" "$gminor" "$gmicro" "$grev"
echo "$gver" > .version
gnuize
cd ..
ls
tar -cvf ${dst_name}.tar $dst_name

# gzip -9 -c ${dst_name}.tar > $dst_name.tar.gz || echo "gzip not available"
bzip2 -z -k ${dst_name}.tar || echo "bzip2 not available"
# xz -z -9 -k ${dst_name}.tar || echo "xz / xz-utils not available"

rm -rf ${dst_name}.tar $dst_dir

mkdir -p ${src_repo}/src_dist
mv -f ${dst_name}.tar.* ${src_repo}/src_dist

cat 1>&2 <<EOF
----------------------------------------------------------------------
The freeswitch-${cver} tarballs have been rolled,
now we just need to roll packages with them
----------------------------------------------------------------------
EOF

