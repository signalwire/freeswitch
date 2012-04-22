======================================
win32/README.txt / Sofia-SIP for win32
======================================

The win32 subdirectory contains the build environment for Win32
environment using MSVC.

Currently, the provided workspace/solution files will create both
shared and static libraries for libsofia-sip-ua, and a few test
programs. The test programs can be run with the script check.cmd.


Preparing the build tree
------------------------

The script autogen.cmd should be used to prepare source tree before
compiling Sofia SIP. Note that it uses AWK, so an AWK interpreter
is needed. You can for example use gawk (3.1.5 or newer) or mawk (tested
with 1.3.3) implementations. Precompiled binaries are available
for instance at:

  - http://gnuwin32.sourceforge.net/packages/mawk.htm
  - http://ftp.uni-kl.de/pub/windows/cygwin/release/gawk/
  - plus many sites, a web search for "win32 awk" will
    provide you many more links


Notes for pthread support
-------------------------

There is a pthread implementation for Visual C on Win32 included.
Source code and documentation for the pthread library can also be
downloaded from http://sources.redhat.com/pthreads-win32/.


MS-VC6 specific notes
---------------------

The MSVC6 workspace file is "SofiaSIP.dsw".

With Visual Studio 6, and Windows 2000 or older Platform SDK, you need to
have the header files from the so called "IPv6 Technology Preview for
Windows 2000" installed, in order to compile Sofia-SIP. This is required
even if IPv6 support is disabled (the socket APIs of older Platfrom SDKs are
insufficient).

You can download the preview SDK from Microsoft Download Center:

http://www.microsoft.com/downloads/

Search for "IPv6 Technology Preview for Windows 2000".

With newer Platform SDKs, the IPv6 Preview SDK is not needed.


MSVC2005 specific notes
------------------------

The MSVC2005 solution file is "SofiaSIP.sln".
