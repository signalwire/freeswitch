@ECHO OFF

REM ##################################################################
REM # set the path/settings of your compiler enviroment and remove the
REM # comment command (REM)
REM # (you don't need this, if you set it always in your system
REM # enviroment)
REM ##################################################################
REM CALL "c:\Programme\Microsoft Visual C++ Toolkit 2003\vcvars32.bat"

REM ##################################################################
REM # set the path to your python24 (or python23) library, for example
REM # works for me with C:\Program Files\Python2.4\libs\python24.lib
REM ##################################################################
SET PYTHON_LIB="C:\Program Files\Python2.4\libs\python24.lib"

REM ##################################################################
REM # set the include path of your python24 (python23) deleveloper
REM # header files. For me, it's on C:\Program Files
REM ################################################################## 
SET PYTHON_INCLUDE="C:\Program Files\Python2.4\include"

REM ##################################################################
REM # after installing swig, set the path, so the script can find it
REM ################################################################## 
SET SWIG="C:\Program Files\swigwin-1.3.40\swig"

REM ##################################################################
REM # DON'T CHANGE ANYMORE AT THE FOLLOWING LINES!!!
REM ##################################################################

SET GPIB_LIB=
SET GPIB_SRC=

ECHO // This file is created automatically, don't change it! > wxctb.i
ECHO %%module wxctb >> wxctb.i
ECHO typedef int size_t; >> wxctb.i
ECHO %%include timer.i >> wxctb.i
ECHO %%include serport.i >> wxctb.i
ECHO %%include ../kbhit.i >> wxctb.i

IF NOT [%1]==[USE_GPIB] GOTO nogpib
SET GPIB_LIB=../../../lib/gpib32.lib
SET GPIB_SRC=../../../src/gpib.cpp
ECHO %%include ../gpib.i >> wxctb.i

:nogpib


DEL *.obj wxctb_wrap.cxx *.lib *.dll *.exp

ECHO "swig generates python wrapper files..."
%SWIG% -c++ -Wall -nodefault -python -keyword -new_repr -modern wxctb.i

ECHO "create shared library wxctb for python 2.4..."
cl /LD /D WIN32 /I %PYTHON_INCLUDE% /I ../../../include wxctb_wrap.cxx ../../../src/win32/serport.cpp ../../../src/serportx.cpp ../../../src/win32/timer.cpp ../../../src/kbhit.cpp ../../../src/iobase.cpp %GPIB_SRC% ../../../src/fifo.cpp /link %PYTHON_LIB% winmm.lib %GPIB_LIB%

MOVE wxctb_wrap.dll _wxctb.dll

ECHO "copy ctb.py, wxctb.py and _wxctb.so to the module/win32 folder..."
MKDIR ..\..\module\win32
COPY ..\ctb.py ..\..\module\win32
COPY wxctb.py ..\..\module\win32
COPY _wxctb.dll ..\..\module\win32

