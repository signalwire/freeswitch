#!/bin/sh
#
# FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
# Copyright (C) 2005-2016, Anthony Minessale II <anthm@freeswitch.org>
#
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/F
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
#
# The Initial Developer of the Original Code is
# Michael Jerris <mike@jerris.com>
# Portions created by the Initial Developer are Copyright (C)
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
#  Sergey Safarov <s.safarov@gmail.com>
#

BUILD_ROOT=/tmp/freeswitch
FILELIST=/tmp/filelist
FILELIST_BINARY=/tmp/filelist_binary
WITHOUT_PERL="true"
WITHOUT_PYTHON="true"
WITHOUT_JAVA="true"
TMP_TAR=/tmp/freeswitch_min.tar.gz
IMG_TAR=/tmp/freeswitch_img.tar.gz

clean_build_root() {
    rm -Rf $BUILD_ROOT
    mkdir -p $BUILD_ROOT
    rm -f $TMP_TAR
    rm -f $IMG_TAR
}

fs_files_debian() {
    local PACKAGES
    PACKAGES=$(dpkg-query -f '${binary:Package}\n' -W 'freeswitch*')
    PACKAGES="libc6 $PACKAGES"
    for pkg in $PACKAGES
    do
        dpkg-query -L $pkg 2> /dev/null
    done
}

extra_files_debian() {
    cat << EOF
/etc
/bin
/bin/busybox
/usr/bin
/usr/bin/epmd
/usr/lib
/usr/lib/erlang
/usr/lib/erlang/bin
/usr/lib/erlang/bin/epmd
/usr/lib/erlang/erts-6.2
/usr/lib/erlang/erts-6.2/bin
/usr/lib/erlang/erts-6.2/bin/epmd
EOF
}

sort_filelist() {
    sort $FILELIST | uniq > $FILELIST.new
    mv -f $FILELIST.new $FILELIST
}

filter_unnecessary_files() {
# excluded following files and directories recursive
# /.
# /lib/systemd/
# /usr/share/doc/
# /usr/share/lintian/
# /usr/share/freeswitch/sounds/
# all "*.flac" files

    sed -i \
        -e '\|^/\.$|d' \
        -e '\|^/lib/systemd|d' \
        -e '\|^/usr/share/doc|d' \
        -e '\|^/usr/share/lintian|d' \
        -e '\|^/usr/share/freeswitch/sounds/|d' \
        -e '\|^/.*\.flac$|d' \
        -e '\|^/.*/flac$|d' \
        $FILELIST

# if disabled Perl and python removing this too
    if [ "$WITHOUT_PERL"="true" ];then
        sed -i -e '\|^/usr/share/perl5|d' $FILELIST
    fi
    if [ "$WITHOUT_PYTHON"="true" ];then
        sed -i -e '\|^/usr/share/pyshared|d' -e '\|^/usr/share/python-support|d' $FILELIST
    fi
    if [ "$WITHOUT_JAVA"="true" ];then
        sed -i -e '\|^/usr/share/freeswitch/scripts/freeswitch.jar|d' $FILELIST
    fi
}

ldd_helper() {
    TESTFILE=$1
    ldd $TESTFILE 2> /dev/null > /dev/null || return

    RESULT=$(ldd $TESTFILE | grep -oP '\s\S+\s\(\S+\)' | sed -e 's/^\s//' -e 's/\s.*$//') #'
# This for tests
#    echo $TESTFILE
    echo "$RESULT"
}

find_binaries() {
    rm -f $FILELIST_BINARY
    for f in $(cat $FILELIST)
    do
        ldd_helper $f >> $FILELIST_BINARY
    done
    sort $FILELIST_BINARY | sort | uniq | sed -e '/linux-vdso.so.1/d' > $FILELIST_BINARY.new
    mv -f $FILELIST_BINARY.new $FILELIST_BINARY
    cat $FILELIST_BINARY | xargs realpath > $FILELIST_BINARY.new
    cat $FILELIST_BINARY.new >> $FILELIST_BINARY
    rm -f $FILELIST_BINARY.new
}

tar_files() {
    local TARLIST=/tmp/tarlist
    cat $FILELIST > $TARLIST
    cat $FILELIST_BINARY >> $TARLIST
    tar -czf $TMP_TAR --no-recursion -T $TARLIST
    rm -f $TARLIST
}

make_image_tar() {
    local CURDIR=`pwd`
    cd $BUILD_ROOT
    tar xzf $TMP_TAR
    find usr/share/freeswitch/conf/* -maxdepth 0 -type d -not -name vanilla -exec rm -Rf {} \;
    # Patching config file
    patch -p 1 < $CURDIR/freeswitch-config.patch
    busybox --install -s bin
    tar czf $IMG_TAR *
    cd $CURDIR
}

apt-get --assume-yes install busybox patch

clean_build_root
fs_files_debian > $FILELIST
extra_files_debian >> $FILELIST
sort_filelist
filter_unnecessary_files
find_binaries
tar_files
make_image_tar
mv $IMG_TAR .
clean_build_root
