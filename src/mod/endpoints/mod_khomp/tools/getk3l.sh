#!/bin/bash

###############################################################################
#   KHOMP generic endpoint/channel library.
#   Copyright (C) 2007-2010 Khomp Ind. & Com.

# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/

# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.

# Alternatively, the contents of this file may be used under the terms of the
# "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
# case the provisions of "LGPL License" are applicable instead of those above.

# If you wish to allow use of your version of this file only under the terms of
# the LGPL License and not to allow others to use your version of this file
# under the MPL, indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by the LGPL
# License. If you do not delete the provisions above, a recipient may use your
# version of this file under either the MPL or the LGPL License.

# The LGPL header follows below:

#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2.1 of the License, or (at your option) any later version.

#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.

#   You should have received a copy of the GNU Lesser General Public License
#   along with this library; if not, write to the Free Software Foundation,
#   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
###############################################################################

K3L_FILE="k3l_2.1_client.sh"
PARAM="0"

if [  "$1" ]
then
    PARAM=$1
fi

help()
{
    echo "Usage: getk3l.sh [OPTION]"
    echo
    echo "  -h,  --help       print this help"
    echo "  -d,  --download   only download the k3l package withou doing the installation" 
    echo 
}

download()
{
    if [ "w`uname -m | grep x86_64`" == "w" ]
    then
        echo "Downloading i686 package"
        wget -t15 -c --progress=bar:force -O $K3L_FILE.gz http://www.khomp.com.br/binaries/softpbx/freeswitch/k3l_2.1_client_i686.sh.gz
    else
        echo "Downloading x86_64 package"
        wget -t15 -c --progress=bar:force -O $K3L_FILE.gz http://www.khomp.com.br/binaries/softpbx/freeswitch/k3l_2.1_client_x86-64.sh.gz
    fi
}

clean()
{
    printf "$1"
    exit 1
}

install()
{
    if [ `whoami` != 'root' ] 
    then
         clean "Need to be root to install !\n"
         exit 1
    fi  

    if ! which 'kserver' &> /dev/null
    then
        download
        gunzip $K3L_FILE.gz
        chmod 0755 $K3L_FILE
        (./$K3L_FILE) || clean "Error on k3l install\n"
        rm $K3L_FILE
    fi
}

if [ $PARAM == '--help' -o $PARAM == '-h' ]
then
    help
    exit 0
elif [ $PARAM == '--download' -o $PARAM == '-d' ]
then 
    download
    exit 0
else
    echo "k3l will be installed"
    install

    if [ "w`kserver --version | grep 2.1`" == "w" ]
    then
        clean "k3l version 2.1 must be installed: \n\tUninstall the old version of k3l and try again\n"
        exit 1
    fi

    echo "Successfully installed!"
fi
