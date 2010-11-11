#!/bin/bash
# this script must be run from openzap root dir and it is assuming
# FreeSWITCH is trunk is located at ../../
fsdir=../..
set -x
cp Debug/mod/*.dll $fsdir/Debug/mod/
cp mod_freetdm/Debug/*.pdb $fsdir/Debug/mod/
cp Debug/*.dll $fsdir/Debug/mod/
cp Debug/*.pdb $fsdir/Debug/mod/
#cp Debug/testsangomaboost.exe $fsdir/Debug/
echo "FRIENDLY REMINDER: RECOMPILE ftmod_wanpipe WHENEVER YOU INSTALL NEW DRIVERS"
set +x 

