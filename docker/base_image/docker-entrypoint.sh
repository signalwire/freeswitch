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

BASEURL=http://files.freeswitch.org
PID_FILE=/var/run/freeswitch/freeswitch.pid

get_password() {
    < /dev/urandom tr -dc _A-Z-a-z-0-9 | head -c${1:-12};echo;
}

get_sound_version() {
    local SOUND_TYPE=$1
    grep "$SOUND_TYPE" sounds_version.txt | sed -E "s/$SOUND_TYPE\s+//"
}

wget_helper() {
    local SOUND_FILE=$1
    grep -q $SOUND_FILE /usr/share/freeswitch/sounds/soundfiles_present.txt 2> /dev/null
    if [ "$?" -eq 0 ]; then
        echo "Skiping download of $SOUND_FILE. Already present"
        return
    fi
    wget $BASEURL/$SOUND_FILE
    if [ -f $SOUND_FILE ]; then
        echo $SOUND_FILE >> /usr/share/freeswitch/sounds/soundfiles_present.txt
    fi
}

download_sound_rates() {
    local i
    local f
    local SOUND_TYPE=$1
    local SOUND_VERSION=$2

    for i in $SOUND_RATES
    do
        f=freeswitch-sounds-$SOUND_TYPE-$i-$SOUND_VERSION.tar.gz
        echo "Downloading $f"
        wget_helper $f
    done
}

download_sound_types() {
    local i
    local SOUND_VERSION
    for i in $SOUND_TYPES
    do
        SOUND_VERSION=$(get_sound_version $i)
        download_sound_rates $i $SOUND_VERSION
    done
}

extract_sound_files() {
    local SOUND_FILES=freeswitch-sounds-*.tar.gz
    for f in $SOUND_FILES
    do
        if [ -f $f ]; then
            echo "Extracting file $f"
            tar xzf $f -C /usr/share/freeswitch/sounds/
        fi
    done
}

delete_archives() {
    local FILES_COUNT=$(ls -1 freeswitch-sounds-*.tar.gz 2> /dev/null | wc -l)
    if [ "$FILES_COUNT" -ne 0 ]; then
        echo "Removing downloaded 'tar.gz' archives"
        rm -f freeswitch-sounds-*.tar.gz
    fi
}

SOUND_RATES=$(echo "$SOUND_RATES" | sed -e 's/:/\n/g')
SOUND_TYPES=$(echo "$SOUND_TYPES" | sed -e 's/:/\n/g')

if [ -z "$SOUND_RATES" -o -z "$SOUND_TYPES" ]; then
	echo "Environment variables 'SOUND_RATES' or 'SOUND_TYPES' not defined. Skiping sound files checking."
else
	download_sound_types
	extract_sound_files
	delete_archives
fi

if [ "$EPMD"="true" ]; then
    /usr/bin/epmd -daemon
fi

if [ ! -f "/etc/freeswitch/freeswitch.xml" ]; then
    SIP_PASSWORD=$(get_password)
    mkdir -p /etc/freeswitch
    cp -varf /usr/share/freeswitch/conf/vanilla/* /etc/freeswitch/
    sed -i -e "s/default_password=.*\?/default_password=$SIP_PASSWORD\"/" /etc/freeswitch/vars.xml
    echo "New FreeSwitch password for SIP calls set to '$SIP_PASSWORD'"
fi

trap '/usr/bin/freeswitch -stop' SIGTERM

/usr/bin/freeswitch -nc -nf -nonat &
pid="$!"

wait $pid
exit 0
