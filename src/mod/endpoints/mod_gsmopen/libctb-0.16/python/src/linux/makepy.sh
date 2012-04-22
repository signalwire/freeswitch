#!/bin/bash

CFLAG=''
GPIB_SOURCES=''
GPIB_LIB=''
GPIB_SUPPORT=''

# the python develop version. Please check the right version of your
# python developent enviroment
PYTHON_VERSION='2.6'

for arg in $*; do
    if [ "$arg" = "USE_GPIB" ]; then
	   GPIB_SOURCES='../../../src/gpib.cpp' 
	   GPIB_LIB='-lgpib'
	   GPIB_SUPPORT='Yes'
    else
	   echo '============================================================'
	   echo 'You run makepy.sh without GPIB support.'
	   echo 'If you want to create the python wxctb library with'
	   echo 'GPIB support, rerun the command with:'
	   echo 'makepy.sh USE_GPIB=1' 
	   echo '============================================================'
	   GPIB_SUPPORT='No'
    fi
    if [ "$arg" = "USE_DEBUG" ]; then
	   CFLAG='-g'
    fi
done

echo "// This file is created automatically, don't change it!" > wxctb.i
echo "%module wxctb" >> wxctb.i
echo "typedef int size_t;" >> wxctb.i
echo "%include timer.i" >> wxctb.i
echo "%include serport.i" >> wxctb.i
echo "%include ../kbhit.i" >> wxctb.i
if [ "$arg" = "USE_GPIB" ]; then
    echo "%include ../gpib.i" >> wxctb.i
fi

echo "swig generates python wrapper files..."
swig -c++ -Wall -nodefault -python -keyword -new_repr -modern wxctb.i

echo "create shared library wxctb with GPIB=$GPIB_SUPPORT for python"\
     "$PYTHON_VERSION ..."
g++ -Wall $CFLAG -shared -I /usr/include/python$PYTHON_VERSION/ \
    -I ../../../include \
    wxctb_wrap.cxx  \
    ../../../src/linux/timer.cpp \
    ../../../src/linux/serport.cpp \
    ../../../src/serportx.cpp \
    ../../../src/kbhit.cpp \
    ../../../src/iobase.cpp \
    ../../../src/fifo.cpp \
    $GPIB_SOURCES \
    $GPIB_LIB \
    -o _wxctb.so

echo "copy ctb.py, wxctb.py and _wxctb.so to the module/linux folder..."
mkdir -p ../../module/linux
cp ../ctb.py ../../module/linux/
cp wxctb.py ../../module/linux/
cp _wxctb.so ../../module/linux/
