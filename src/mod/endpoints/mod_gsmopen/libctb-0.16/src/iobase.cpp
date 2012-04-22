/////////////////////////////////////////////////////////////////////////////
// Name:        iobase.cpp
// Purpose:
// Author:      Joachim Buermann
// Id:          $Id: iobase.cpp,v 1.1.1.1 2004/11/24 10:30:11 jb Exp $
// Copyright:   (c) 2001 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "ctb-0.16/iobase.h"
#include "ctb-0.16/timer.h"

namespace ctb {

#define DELTA_BUFSIZE 512 

    int IOBase::Readv(char* buf,size_t len,unsigned int timeout_in_ms)
    {
	   char *cp = buf;
	   int n = 0;
	   int timeout = 0;
	   size_t toread = len;

	   Timer t(timeout_in_ms,&timeout,NULL);
	   if(timeout_in_ms != 0xFFFFFFFF) {
		  t.start();
	   }

	   while(!timeout && (toread > 0)) {
		  if((n = Read(cp,toread)) < 0) {
			 break;
		  }
		  if(!n) {
			 sleepms(1);
		  }
		  toread -= n;
		  cp += n;
	   }
	   // ok, all bytes received
	   return(len - toread);
    };

/*
  Readv() calls the member function Read() repeatedly, til all
  demand bytes were received. To avoid an endless loop, you
  can refer an integer, which was set unequal zero after a
  specific time. (See the timer class)
*/
    int IOBase::Readv(char* buf,size_t len,int* timeout_flag,bool nice)
    {
	   size_t toread = len;
	   int n = 0;
	   char *cp = buf;

	   while(toread > 0) {
		  if(timeout_flag && (*timeout_flag > 0)) {
			 return (len - toread);
		  }
		  if((n = Read(cp,toread)) < 0) {
			 return (len - toread);
		  }
		  if(!n && nice) {
			 sleepms(1);
		  }
		  if (n > 0)
		  {
			 toread -= n;
			 cp += n;
		  }
	   }
	   // ok, all bytes received
	   return(len - toread);
    };

    int IOBase::ReadUntilEOS(char*& readbuf,
						 size_t* readedBytes,
						 char* eosString,
						 long timeout_in_ms,
						 char quota)
    {
	   int n = 0;
	   int timeout = 0;
	   int bufsize = DELTA_BUFSIZE;
	   int result = 0;
	   int quoted = 0;
	   char* buf = new char[bufsize];
	   char* des = buf;
	   char* eos = eosString;
	   char ch;

	   Timer t(timeout_in_ms,&timeout,NULL);
	   t.start();

	   while(!timeout) {
		  if(des >= &buf[bufsize]) {
			 // buffer full, realloc more memory
			 char* tmp = new char[bufsize + DELTA_BUFSIZE + 1];
			 memcpy(tmp,buf,bufsize);
			 delete[] buf;
			 buf = tmp;
			 des = &buf[bufsize];
			 bufsize += DELTA_BUFSIZE;
		  }
		  // read next byte
		  n = Read(&ch,1);
		  if(n < 0) {
			 // an error occured
			 result = -1;
			 break;
		  }
		  else if(n == 0) {
			 // no data available, give up the processor for some time
			 // to reduce the cpu last
			 sleepms(10);
			 continue;
		  }
		  // if eos is composed of more than one char, and the current
		  // byte doesn't match the next eos character, we handle the
		  // readed byte as a normal char (and not an eos)
		  if((eos != eosString) && (ch != *eos)) {
			 // FIXME!
			 // write all characters, which was matched the eos string
			 // until now (with the first wrong character all received
			 // eos characters are invalid and must handled as normal
			 // characters).

			 // This doesn't work right and is only a little workaround
			 // because the received eos chars are lost
			 PutBack(ch);
			 // because we doesn't match the eos string, we must 'reset' 
			 // the eos match
			 eos = eosString;
			 continue;
		  }
		  else {
			 if((ch == *eos) && !quoted) {
				if(*++eos == 0) {
				    // the eos string is complete
				    result = 1;
				    break;
				}
				continue;
			 }
		  }
		  if(ch == quota) {
			 quoted ^= 1;
		  }
		  *des++ = ch;
	   }
	   *des = 0;
	   readbuf = buf;
	   *readedBytes = des - buf;
	   return result;
    };

    int IOBase::Writev(char* buf,size_t len,unsigned int timeout_in_ms)
    {
	   char *cp = buf;
	   int n = 0;
	   int timeout = 0;
	   size_t towrite = len;

	   Timer t(timeout_in_ms,&timeout,NULL);
	   if(timeout_in_ms != 0xFFFFFFFF) {
		  t.start();
	   }

	   while(!timeout && (towrite > 0)) {
		  if((n = Write(cp,towrite)) < 0) {
			 // an error occurs
			 break;
		  }
		  if(!n) {
			 sleepms(1);
		  }
		  towrite -= n;
		  cp += n;
	   }
	   return (len - towrite);
    };

/*
  Similar to Readv(). Writev() calls Write() repeatedly till
  all bytes are written.
*/
    int IOBase::Writev(char* buf,size_t len,int* timeout_flag,bool nice)
    {
	   size_t towrite = len;
	   int n = 0;
	   char *cp = buf;

	   while(towrite > 0) {
		  if(timeout_flag && (*timeout_flag > 0)) {
			 return (len - towrite);
		  }
		  if((n = Write(cp,towrite)) < 0) {
			 // an error occurs
			 return (len - towrite);
		  }
		  if(!n && nice) {
			 sleepms(1);
		  }
		  towrite -= n;
		  cp += n;
	   }
	   return(len);
    };

} // namespace ctb
