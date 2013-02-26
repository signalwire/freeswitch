#!/bin/bash
# ./menu.sh
#
# David Rowe
# Created August 2009
#
# Presents a menu of sound files, press 1 to play file1, 2 to play file2 etc
#
# The aim is to make comparing files with different processing easier than
# using up-arrow on the command line.  Based on cdialog.
#
# usage:
#   menu.sh file1.raw file2.raw ........ [-d playbackdevice]
#
# for example:
#
#   ../script/menu.sh hts1a.raw hts1a_uq.raw 
#
# or:
#
#   ../script/menu.sh hts1a.raw hts1a_uq.raw -d /dev/dsp1
#

#  Copyright (C) 2007 David Rowe
# 
#  All rights reserved.
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2, as
#  published by the Free Software Foundation.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.

files=0
items="Q-Quit\n"
while [ ! -z "$1" ]
do
  case "$1" in
    -d) dsp="${1} ${2}"; shift;;
     *) files=`expr 1 + $files`;
        new_file=$1;
        file[$files]=$new_file;
        items="${items} ${files}-${new_file}\n";;
  esac
  shift
done

echo -n -e "\r" $items"- "
while true ; do
  echo -n -e "\r -"
  stty cbreak         # or stty raw. Stty uses file descriptor 0, not /dev/tty.
  readchar=`dd bs=1 count=1 2>/dev/null`
  stty -cbreak
  if [ -n "$readchar" ] ; then
    if [ x$readchar == 'xq' -o x$readchar == 'xQ' ] ; then
      echo
      exit 0
    fi
    if [ -z ${file[$readchar]} ] ; then
        echo -n -e "\nUnknown input\n" $items"- "
        continue
    fi
    if ( play --version ) >/dev/null 2>&1; then
      play -r 8000 -s -2 ${file[$readchar]} $dsp 2> /dev/null
    elif ( aplay --version ) > /dev/null 2>&1; then
      aplay -r 8000 -f S16_LE ${file[$readchar]} 2> /dev/null
    elif ( ossplay -f? ) > /dev/null 2>&1; then
      ossplay -s8000 -fS16_LE ${file[$readchar]} 2> /dev/null
    else
      echo "could not find play, aplay or ossplay program"
    fi
  fi
done
echo
