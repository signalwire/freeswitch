/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2006 Jori Liesenborgs

  Contact: jori@lumumba.uhasselt.be

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.uhasselt.be), a research center of the Hasselt University
  (http://www.uhasselt.be). The library is based upon work done for 
  my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

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

*/

#ifndef RTPLIBRARYVERSION_H

#define RTPLIBRARYVERSION_H

#include <string>
#include <stdio.h>

class RTPLibraryVersion
{
public:
      	static RTPLibraryVersion GetVersion();
private:
	RTPLibraryVersion(int major,int minor,int debug) 			{ majornr = major; minornr = minor; debugnr = debug; }
public:
	int GetMajorNumber() const						{ return majornr; }
	int GetMinorNumber() const						{ return minornr; }
	int GetDebugNumber() const						{ return debugnr; }
	std::string GetVersionString() const;
private:
	int debugnr,minornr,majornr;
};

inline std::string RTPLibraryVersion::GetVersionString() const
{
	char str[16];
	
#if (defined(WIN32) || defined(_WIN32_WCE))
	_snprintf(str,16,"%d.%d.%d",majornr,minornr,debugnr);
#else
	snprintf(str,16,"%d.%d.%d",majornr,minornr,debugnr);
#endif // WIN32 || _WIN32_WCE
	return std::string(str);
}

#endif // RTPLIBRARYVERSION_H

