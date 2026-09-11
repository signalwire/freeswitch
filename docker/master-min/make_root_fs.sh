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
#  David Heaps <king.dopey.10111@gmail.com>
#

BUILD_ROOT=/tmp/newroot
DEBFILELIST=/tmp/filelist
PACKAGELIST="libc6 busybox erlang erlang-base ca-certificates openssl gnupg2 passwd curl"
PACKAGESEARCH="freeswitch"
DEBFILELIST_BINARY="$DEBFILELIST.binary"
DEBFILELISTLINKED="$DEBFILELIST.full.linked"
FULLLIST="$DEBFILELIST.full"
FOLDERLIST="$FULLLIST.folder"
FOLDERLINKLIST="$FOLDERLIST.link"
FULLFILELIST="$FULLLIST.file"
WITHOUT_PERL="true"
WITHOUT_PYTHON="true"
WITHOUT_JAVA="true"

filter_unnecessary_files() {
# excluded following files and directories recursive
# /.
# /lib/systemd/
# /usr/share/doc/
# /usr/share/man/
# /usr/share/lintian/
# /usr/share/freeswitch/sounds/
# all "*.flac" files

    sed -i \
        -e '\|^/\.$|d' \
        -e '\|^/lib/systemd|d' \
        -e '\|^/usr/share/doc|d' \
        -e '\|^/usr/share/man|d' \
        -e '\|^/usr/share/lintian|d' \
        -e '\|^/usr/share/freeswitch/sounds/|d' \
        -e '\|^/.*\.flac$|d' \
        -e '\|^/.*/flac$|d' \
        $FULLLIST

# if disabled Perl and python removing this too
    if [ "$WITHOUT_PERL" = "true" ];then
        sed -i -e '\|^/usr/share/perl5|d' $FULLLIST
    fi
    if [ "$WITHOUT_PYTHON" = "true" ];then
        sed -i -e '\|^/usr/lib/python3|d' -e '\|^/usr/share/pyshared|d' -e '\|^/usr/share/python-support|d' -e '\|^/lib/x86_64-linux-gnu/libpython3|d' -e '\|^/usr/lib/x86_64-linux-gnu/libpython|d' $FULLLIST
    fi
    if [ "$WITHOUT_JAVA" = "true" ];then
        sed -i -e '\|^/usr/share/freeswitch/scripts/freeswitch.jar|d' $FULLLIST
    fi
}

fs_files_debian() {
    PACKAGES="$PACKAGELIST"
    if [ "$WITHOUT_PERL" = "false" ];then
        PACKAGES="$PACKAGES perl-base"
    fi
    if [ "$WITHOUT_PYTHON" = "false" ];then
        PACKAGES="$PACKAGES python3 python3.11-minimal"
    fi
    if [ "$WITHOUT_JAVA" = "false" ];then
        PACKAGES="$PACKAGES openjdk-17-jre-headless java-common"
    fi
    for search in $PACKAGESEARCH; do
        NEW_PACKAGES=$(dpkg-query -f '${binary:Package}\n' -W "*$search*")
        PACKAGES="$NEW_PACKAGES $PACKAGES"
    done

    for pkg in $PACKAGES
    do
        dpkg-query -L "$pkg" >> $DEBFILELIST 2> /dev/null
    done
}

dpkg_search_cmd() {
    dpkg-query -f '\${binary:Package}\n' -W "*$1*"
}

clean_build() {
    rm -Rf $BUILD_ROOT
    mkdir -p $BUILD_ROOT
    rm -f $DEBFILELIST
    rm -f $DEBFILELIST_BINARY
    rm -f $DEBFILELISTLINKED
    rm -f $FULLLIST
    rm -f $FOLDERLIST
    rm -f $FOLDERLINKLIST
    rm -f $FULLFILELIST
}

sort_filelist() {
    sort "$1" | uniq > "$1".new
    mv -f "$1".new "$1"
}

ldd_helper() {
    TESTFILE=$1
    ldd "$TESTFILE" 2> /dev/null > /dev/null || return
    RESULT=$(ldd "$TESTFILE" | grep -oP '\s\S+\s\(\S+\)' | sed -e 's/^\s//' -e 's/\s.*$//') #'
    echo "$RESULT"
}

find_binaries() {
    cat $DEBFILELIST | while IFS= read -r f
    do
        ldd_helper "$f" >> $DEBFILELIST_BINARY
    done

    sort $DEBFILELIST_BINARY | sort | uniq | sed -e '/linux-vdso.so.1/d' > $DEBFILELIST_BINARY.new
    mv -f $DEBFILELIST_BINARY.new $DEBFILELIST_BINARY
    cat $DEBFILELIST_BINARY | xargs realpath > $DEBFILELIST_BINARY.new
    cat $DEBFILELIST_BINARY.new >> $DEBFILELIST_BINARY
    rm -f $DEBFILELIST_BINARY.new
}

symlink_helper() {
    TESTFILE=$1
    RESULT=$(readlink "$TESTFILE")
    [ -z "$RESULT" ] ||
    cd "$(dirname "$TESTFILE")" &&
    RESULT=$(realpath "$RESULT" 2> /dev/null)
    [ -z "$RESULT" ] || echo "$RESULT"
}

follow_symlinks() {
    cat $FULLLIST | while IFS= read -r f
    do
        symlink_helper "$f" >> $DEBFILELISTLINKED
    done
}

create_folder_structure() {
    #Create the directory/folder structure first
    #This is to prevent confusion with symlinked folders and allow for a simpler copy
    cat $FULLLIST | while IFS= read -r f
    do
        FOLDER_TO_CREATE=""
        if [ -d "$f" ]; then
            FOLDER_TO_CREATE="$f"
        else
            FOLDER_TO_CREATE=$(dirname "$f")
            if [ -n "$f" ]; then
                 echo "$f" >> $FULLFILELIST
            fi
        fi
        #Check if folder is a link
        if [ -L "$FOLDER_TO_CREATE" ]; then
            echo "$FOLDER_TO_CREATE" >> $FOLDERLINKLIST
        else
            echo "$FOLDER_TO_CREATE" >> $FOLDERLIST
        fi
    done

    sort_filelist $FOLDERLIST
    sort_filelist $FOLDERLINKLIST

    #Create links first, to prevent folder creation of a child, which was a link
    cat $FOLDERLINKLIST | while IFS= read -r f
    do
        #Create the folder it's linking to at the same time, to prevent racing conditions
        FOLDER_TO_CREATE=$(readlink "$f")
        if [ -n "$BUILD_ROOT$FOLDER_TO_CREATE" ]; then
            mkdir -p "$BUILD_ROOT$FOLDER_TO_CREATE"
            chown --reference="$FOLDER_TO_CREATE" "$BUILD_ROOT$FOLDER_TO_CREATE"
            chmod --reference="$FOLDER_TO_CREATE" "$BUILD_ROOT$FOLDER_TO_CREATE"
        fi

        #Get the parent folder of the link to allow for deep references
        PARENT_FOLDER=$(dirname "$f")
        if [ -n "$BUILD_ROOT$PARENT_FOLDER" ]; then
            mkdir -p "$BUILD_ROOT$PARENT_FOLDER"
            chown --reference="$PARENT_FOLDER" "$BUILD_ROOT$PARENT_FOLDER"
            chmod --reference="$PARENT_FOLDER" "$BUILD_ROOT$PARENT_FOLDER"
        fi
        cp -pP "$f" "$BUILD_ROOT$PARENT_FOLDER"
    done

    #Create all remaining folders
    cat $FOLDERLIST | while IFS= read -r f
    do 
        if [ ! -e "$BUILD_ROOT$f" ]; then
            mkdir -p "$BUILD_ROOT$f"
            chown --reference="$f" "$BUILD_ROOT$f"
            chmod --reference="$f" "$BUILD_ROOT$f"
        fi
    done
}

create_full_file_list() {
    cat $DEBFILELIST > $FULLLIST
    find_binaries
    cat $DEBFILELIST_BINARY >> $FULLLIST
    follow_symlinks
    cat $DEBFILELISTLINKED >> $FULLLIST
    sort_filelist $FULLLIST
}

copy_files() {
    #Note that creating the folder stucture also creates FULLLIST.file, which excludes said folders
    cat $FULLFILELIST | while IFS= read -r f
    do
        cp -pP "$f" "$BUILD_ROOT$f" 
    done
}

make_new_root() {
    cd $BUILD_ROOT || exit
    cp -p /usr/local/bin/su-exec usr/bin
    find usr/share/freeswitch/conf/* -maxdepth 0 -type d -not -name vanilla -exec rm -Rf {} \;
    # Patching config file
    patch -p 1 < /freeswitch-config.patch
    mkdir bin
    busybox --install -s bin
    cp -rpP /etc/ssl/certs etc/ssl
    mkdir -p etc/pki/tls/certs
    cp etc/ssl/certs/ca-certificates.crt etc/pki/tls/certs/ca-bundle.crt
}

CUR_DIR=$(pwd)
clean_build
fs_files_debian
sort_filelist $DEBFILELIST
create_full_file_list
filter_unnecessary_files
create_folder_structure
copy_files
make_new_root
cd "$CUR_DIR" || exit