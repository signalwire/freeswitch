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

#include "rtprandom.h"
#include <time.h>
#ifndef WIN32
	#include <unistd.h>
#else
	#ifndef _WIN32_WCE
		#include <process.h>
	#else
		#include <windows.h>
		#include <kfuncs.h>
	#endif // _WIN32_WINCE
	#include <stdlib.h>
#endif // WIN32

#include "rtpdebug.h"

#if !defined(RTP_SUPPORT_GNUDRAND) && !defined(RTP_SUPPORT_RANDR)
bool RTPRandom::init = false;
#endif // WIN32

RTPRandom::RTPRandom()
{
#if defined(RTP_SUPPORT_GNUDRAND) || defined(RTP_SUPPORT_RANDR)
	uint32_t x;

	x = (uint32_t)getpid();
	x += (uint32_t)time(0);
	x -= (uint32_t)clock();
	x ^= (uint32_t)(this);

#ifdef RTP_SUPPORT_GNUDRAND
	srand48_r(x,&drandbuffer);
#else
	state = (unsigned int)x;
#endif
	
#else // use simple rand and srand functions
	if (init)
		return;

	uint32_t x;

#ifndef _WIN32_WCE
	x = (uint32_t)getpid();
	x += (uint32_t)time(0);
	x -= (uint32_t)clock();
#else
	x = (uint32_t)GetCurrentProcessId();

	FILETIME ft;
	SYSTEMTIME st;
	
	GetSystemTime(&st);
	SystemTimeToFileTime(&st,&ft);
	
	x += ft.dwLowDateTime;
#endif // _WIN32_WCE
	x ^= (uint32_t)(this);
	srand((unsigned int)x);

	init = true;
#endif
}

RTPRandom::~RTPRandom()
{
}

#ifdef RTP_SUPPORT_GNUDRAND

uint8_t RTPRandom::GetRandom8()
{
	double x;
	drand48_r(&drandbuffer,&x);
	uint8_t y = (uint8_t)(x*256.0);
	return y;
}

uint16_t RTPRandom::GetRandom16()
{
	double x;
	drand48_r(&drandbuffer,&x);
	uint16_t y = (uint16_t)(x*65536.0);
	return y;
}

uint32_t RTPRandom::GetRandom32()
{
	uint32_t a = GetRandom16();
	uint32_t b = GetRandom16();
	uint32_t y = (a << 16)|b;
	return y;
}

double RTPRandom::GetRandomDouble()
{
	double x;
	drand48_r(&drandbuffer,&x);
	return x;
}

#else 
#ifdef RTP_SUPPORT_RANDR

uint8_t RTPRandom::GetRandom8()
{
	uint8_t x;

	x = (uint8_t)(256.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	return x;
}

uint16_t RTPRandom::GetRandom16()
{
	uint16_t x;

	x = (uint16_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	return x;
}

uint32_t RTPRandom::GetRandom32()
{
	uint32_t x,y;

	x = (uint32_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	y = x;
	x = (uint32_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	y ^= (x<<8);
	x = (uint32_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	y ^= (x<<16);

	return y;
}

double RTPRandom::GetRandomDouble()
{
	double x = ((double)rand_r(&state))/((double)RAND_MAX+1.0);
	return x;
}

#else

uint8_t RTPRandom::GetRandom8()
{
	uint8_t x;

	x = (uint8_t)(256.0*((double)rand())/((double)RAND_MAX+1.0));
	return x;
}

uint16_t RTPRandom::GetRandom16()
{
	uint16_t x;

	x = (uint16_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	return x;
}

uint32_t RTPRandom::GetRandom32()
{
	uint32_t x,y;

	x = (uint32_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	y = x;
	x = (uint32_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	y ^= (x<<8);
	x = (uint32_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	y ^= (x<<16);

	return y;
}

double RTPRandom::GetRandomDouble()
{
	double x = ((double)rand())/((double)RAND_MAX+1.0);
	return x;
}

#endif // RTP_SUPPORT_RANDR
#endif // RTP_SUPPORT_GNUDRAND

