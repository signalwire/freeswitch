// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_unix_port.h
// *
// * Purpose: UNIX serial port implementation
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.5.1999
// *************************************************************************

#ifndef GSM_UNIX_SERIAL_H
#define GSM_UNIX_SERIAL_H

#include <string>
#include <gsmlib/gsm_error.h>
#include <gsmlib/gsm_port.h>
#include <gsmlib/gsm_util.h>
#include <sys/types.h>
#include <termios.h>

using namespace std;

namespace gsmlib
{
  class UnixSerialPort : public Port
  {
  private:
    int _fd;                    // file descriptor for device
    int _debug;                 // debug level (set by environment variable
                                // GSM_DEBUG
    int _oldChar;               // character set by putBack() (-1 == none)
    long int _timeoutVal;       // timeout for getLine/readByte

    // throw GsmException include UNIX errno
    void throwModemException(string message) throw(GsmException);
    
  public:
    // create Port given the UNIX device name
    UnixSerialPort(string device, speed_t lineSpeed = DEFAULT_BAUD_RATE,
                   string initString = DEFAULT_INIT_STRING,
                   bool swHandshake = false)
      throw(GsmException);

    // inherited from Port
    void putBack(unsigned char c);
    int readByte() throw(GsmException);
    string getLine() throw(GsmException);
    void putLine(string line,
                         bool carriageReturn = true) throw(GsmException);
    bool wait(GsmTime timeout) throw(GsmException);
    void setTimeOut(unsigned int timeout);

    virtual ~UnixSerialPort();
  };

  // convert baudrate string ("300" .. "460800") to speed_t
  extern speed_t baudRateStrToSpeed(string baudrate) throw(GsmException);
};

#endif // GSM_UNIX_SERIAL_H
