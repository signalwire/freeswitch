======================================
win32/README.txt / Sofia-SIP for win32
======================================

The win32 subdirectory contains the build environment for Win32 environment
using MSVC. In order to compile the code with Windows 2000 SDK you have to
install so called "IPv6 Technology Preview for Windows 2000". The preview
contains updated IPv6 API in <tpipv6.h> and <wspiapi.h> header files.

http://msdn.microsoft.com/downloads/sdks/platform/tpipv6.asp

There is a pthread implementation for Visual C on Win32 included.
Source code and documentation for the pthread library can also be
downloaded from http://sources.redhat.com/pthreads-win32/.

The script autogen.cmd should be used to prepare source tree before
compiling Sofia SIP. Note that it uses the gawk utility - see
http://unxutils.sourceforge.net.

Currently, the SofiaSIP.dsw workspace creates a shared library for
sofia-sip-ua and a few test programs. The tests programs can be run 
with the script check.cmd.
