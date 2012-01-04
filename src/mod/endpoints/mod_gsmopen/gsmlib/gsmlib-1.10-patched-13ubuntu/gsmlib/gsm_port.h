// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_port.h
// *
// * Purpose: Abstract port definition
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.5.1999
// *************************************************************************

#ifndef GSM_PORT_H
#define GSM_PORT_H

#include <gsmlib/gsm_error.h>
#include <gsmlib/gsm_util.h>
#include <string>

using namespace std;

namespace gsmlib
{
  // TA defaults
  const int TIMEOUT_SECS = 60;
  const char DEFAULT_INIT_STRING[] = "E0";
  const int DEFAULT_BAUD_RATE = 38400;

  class Port : public RefBase
  {
  public:
    // read line from port(including eol characters)
    virtual string getLine() throw(GsmException) =0;
    
    // write line to port
    virtual void putLine(string line,
                         bool carriageReturn = true) throw(GsmException) =0;

    // wait for new data to become available, return after timeout
    // if timeout == 0, wait forever
    // return true if data available
    virtual bool wait(GsmTime timeout) throw(GsmException) =0;

    // put back one byte that can be read by a subsequent call to readByte()
    virtual void putBack(unsigned char c) =0;

    // read a single byte, return -1 if error or file closed
    virtual int readByte() throw(GsmException) =0;

    // set timeout for the readByte(), getLine(), and putLine() functions
    // (globally for ALL ports)
    virtual void setTimeOut(unsigned int timeout) =0;

    virtual ~Port() {}
  };
};

#endif // GSM_PORT_H
