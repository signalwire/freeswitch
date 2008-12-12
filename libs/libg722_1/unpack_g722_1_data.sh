#!/bin/sh
#
# g722_1 - a library for the G.722.1 and Annex C codecs
#
# unpack_g722_1_data.sh
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 2.1,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: unpack_g722_1_data.sh,v 1.2 2008/09/26 12:09:29 steveu Exp $
#

ITUDATA="../../../T-REC-G.722.1-200505-I!!SOFT-ZST-E.zip"

cd test-data/itu
if [ -d g722_1 ]
then
    cd g722_1
else
    mkdir g722_1
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/itu/g722_1!
        exit $RETVAL
    fi
    cd g722_1
fi

if [ -d fixed ]
then
    cd fixed
    rm -rf *
    cd ..
else
    mkdir fixed
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/itu/g722_1/fixed!
        exit $RETVAL
    fi
fi
if [ -d floating ]
then
    cd floating
    rm -rf *
    cd ..
else
    mkdir floating
    RETVAL=$?
    if [ $RETVAL != 0 ]
    then
        echo Cannot create test-data/itu/g722_1/floating!
        exit $RETVAL
    fi
fi

rm -rf T*
rm -rf Software
rm -rf G722-1E-200505+Cor1.pdf
rm -rf G722-1E-200505+Cor1.DOC
rm -rf Software
unzip ${ITUDATA} >/dev/null
RETVAL=$?
if [ $RETVAL != 0 ]
then
    echo Cannot unpack the ITU test vectors for G.722.1!
    exit $RETVAL
fi
#rm ${ITUDATA}
mv ./Software/Fixed-200505-Rel.2.1/vectors/* ./fixed
mv ./Software/Floating-200806-Rel.2.1/vectors/* ./floating
rm -rf Software
rm -rf G722-1E-200505+Cor1.pdf
rm -rf G722-1E-200505+Cor1.DOC
echo The ITU test vectors for G.722.1 should now be in the g722_1 directory

