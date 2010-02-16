#!/bin/bash
# this script must be run from openzap root dir and it is assuming
# FreeSWITCH is trunk is located at ../../
fsdir=../..
set -x
cp Debug/*.dll $fsdir/Debug/
cp Debug/mod/*.dll $fsdir/Debug/mod/
cp Debug/*.pdb $fsdir/Debug/
echo "FRIENDLY REMINDER: RECOMPILE ftmod_wanpipe WHENEVER YOU INSTALL NEW DRIVERS"
set +x 

