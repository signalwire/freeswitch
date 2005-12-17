------------------------------------------------------------------------------
                                                               October 2, 2005


                                   JRTPLIB (v3.3.0)


                      Developed at the The Expertise Centre for 
                      Digital Media (EDM), a research institute  
                              of the Hasselt University

                             http://www.edm.uhasselt.be/
                               http://www.uhasselt.be/


LIBRARY LOCATION AND CONTACT
============================
Normally, you should be able to download the latest version of the library
from this url:
	http://research.edm.uhasselt.be/jori/jrtplib/jrtplib.html

If you have questions about the library, you can mail me at:
	jori@lumumba.uhasselt.be
	
There is also a mailing list for the library. To subscribe to the list, 
send an e-mail with the text 'subscribe jrtplib' as the message body (not 
the subject) to majordomo@edm.uhasselt.be and you'll receive further 
instructions.

ACKNOWLEDGMENT
==============
I would like thank the people at the Expertise Centre for Digital Media
for giving me the opportunity to create this rewrite of the library.

DISCLAIMER & COPYRIGHT
======================

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

INSTALLATION NOTES
==================

* To compile the library on a unix-like platform, just start the configure
  script. This should generate a makefile which you can use to start
  building the library using 'make'.

* To compile the library on an MS-Windows platfrom: Visual Studio
  workspace files are included. The project will search
  for JThread files in ..\jthread-1.1.2
  To compile the library without using JThread, comment the line
  with RTP_SUPPORT_THREAD in rtpconfig_win.h
  Note that for Visual Studio 6, you must have service pack 6 for 
  installed to be able to compile the library.  (NOTE:  In this copy
  of the lib, jthread has been included and does not need to be included
  seperately)

* For a manual about the library, please refer to doc/ subdirectory.
  Using 'make doc', a PDF version of the documentation will be built.
  Note that you'll need to have the pdflatex LaTeX compiler installed.
  Alternatively, you can download the documentation from the homepage:
  http://research.edm.uhasselt.be/jori/jrtplib/jrtplib.pdf

* For systems with low memory or for applications which will involve only
  a few participants at a time:
  You can set the HASHSIZE defines in rtpsources.h, rtpudpv4transmitter.h
  and rtpudpv6transmitter.h to a lower value to avoid memory being wasted.
  Note that the library will have to be recompiled.

* Used defines:
	- WIN32:
		For compilation on an Win32 platform.
	- _WIN32_WCE:
		Define needed for compilation on a WinCE platform
	- RTP_HAVE_SYS_FILIO:
		Set if <sys/filio.h> exists.
	- RTP_HAVE_SYS_SOCKIO:
		Set if <sys/sockio.h> exists.
	- RTP_BIG_ENDIAN:
		If set, assume big-endian byte ordering.
	- RTP_SOCKLENTYPE_UINT:
		Indicates that getsockname used an unsigned int as its
		third parameter.
	- RTP_HAVE_SOCKADDR_LEN:
		Indicates that struct sockaddr has an sa_len field.
	- RTP_SUPPORT_IPV4MULTICAST:
		Enables support for IPv4 multicasting.
	- RTP_SUPPORT_THREAD:
		Enables support for JThread.
	- RTP_SUPPORT_SDESPRIV:
		Enables support for RTCP SDES private items.
	- RTP_SUPPORT_INLINETEMPLATEPARAM:
		If set, the compiler should be able to handle an inline
		function as a template parameter.
	- RTP_SUPPORT_PROBATION:
		If set, a few consecutive RTP packets are needed to validate
		a member.
	- RTP_SUPPORT_GNUDRAND:
		If set, the RTPRandom class will use drand48_r and srand48_r
	- RTP_SUPPORT_RANDR:
		If set and RTP_SUPPORT_GNUDRAND is not set, the RTPRandom
		class will use rand_r.
	- RTP_SUPPORT_GETLOGINR:
		If set, the library will use getlogin_r instead of getlogin.
	- RTP_SUPPORT_IPV6:
		If set, IPv6 support is enabled.
	- RTP_SUPPORT_IPV6MULTICAST:
		If set, IPv6 multicasting support is enabled.
	- RTP_SUPPORT_GST:
		If set, GStreamer support is enabled.
	- RTPDEBUG:
		Enables some memory tracking functions and some debug routines.
	
------------------------------------------------------------------------------

