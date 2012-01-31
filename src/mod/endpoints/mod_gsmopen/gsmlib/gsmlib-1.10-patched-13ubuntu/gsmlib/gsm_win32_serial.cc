// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_win32_port.cc
// *
// * Purpose: WIN32 serial port implementation
// *
// * Author:  Frediano Ziglio (freddy77@angelfire.com)
// *
// * Created: 25.10.2000
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <winsock.h>
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_win32_serial.h>
#include <gsmlib/gsm_util.h>
#include <fcntl.h>
#include <iostream>
#include <strstream>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>

using namespace std;
using namespace gsmlib;

static long int timeoutVal = TIMEOUT_SECS;

struct ExceptionSafeOverlapped: public OVERLAPPED
{
  ExceptionSafeOverlapped()
  {
    memset((OVERLAPPED*)this,0,sizeof(OVERLAPPED));
    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL ); 
    if (hEvent == INVALID_HANDLE_VALUE) 
      throw GsmException(_("error creating event"),OSError,GetLastError());
  }
  ~ExceptionSafeOverlapped()
  { CloseHandle(hEvent); }
};

typedef BOOL (WINAPI *TCancelIoProc)(HANDLE file);
TCancelIoProc CancelIoProc = NULL;
BOOL CancelIoHook(HANDLE file)
{
  if (CancelIoProc)
    return CancelIoProc(file);

  HMODULE hmodule = GetModuleHandle("KERNEL32");
  if (hmodule)
  {
    CancelIoProc = (TCancelIoProc)GetProcAddress(hmodule,"CancelIo");
    if (CancelIoProc)
      return CancelIoProc(file);
  }
                         
  return TRUE;
}
#define CancelIo CancelIoHook

// Win32SerialPort members

void Win32SerialPort::throwModemException(string message) throw(GsmException)
{
  ostrstream os;
  os << message << " (errno: " << errno << "/" << strerror(errno) << ")"
     << ends;
  char *ss = os.str();
  string s(ss);
  delete[] ss;
  throw GsmException(s, OSError, errno);
}

void Win32SerialPort::putBack(unsigned char c)
{
  assert(_oldChar == -1);
  _oldChar = c;
}

int Win32SerialPort::readByte() throw(GsmException)
{
  if (_oldChar != -1)
  {
    int result = _oldChar;
    _oldChar = -1;
    return result;
  }

  unsigned char c;
  int timeElapsed = 0;
  bool readDone = true;
  ExceptionSafeOverlapped  over;

  DWORD initTime = GetTickCount();
  DWORD dwReaded;
  if (!ReadFile(_file,&c,1,&dwReaded,&over))
  {
    readDone = false;
    if (GetLastError() != ERROR_IO_PENDING)
    {
      throwModemException(_("reading from TA"));
    }

    while(!readDone)
    {
      if (interrupted())
        throwModemException(_("interrupted when reading from TA"));

      // wait another second
      switch(WaitForSingleObject(over.hEvent,1000))
      {
      case WAIT_TIMEOUT:
        break;
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED:
	      // !!! do a infinite loop if (bytesWritten < lenght) ?
        GetOverlappedResult(_file,&over,&dwReaded,TRUE);
        readDone = true;
        break;
      case WAIT_FAILED:
        throwModemException(_("reading from TA"));
      }

      timeElapsed = (GetTickCount() - initTime)/1000U;

      // timeout elapsed ?
      if (timeElapsed >= timeoutVal)
      {
        CancelIo(_file);
        break;
      }

    }
  }
  
  if (! readDone)
    throwModemException(_("timeout when reading from TA"));

#ifndef NDEBUG
  if (debugLevel() >= 2)
  {
    // some useful debugging code
    if (c == LF)
      cerr << "<LF>";
    else if (c == CR)
      cerr << "<CR>";
    else cerr << "<'" << (char) c << "'>";
    cerr.flush();
  }
#endif
  return c;
}

Win32SerialPort::Win32SerialPort(string device, int lineSpeed,
                               string initString, bool swHandshake)
  throw(GsmException) :
  _oldChar(-1)
{
 try
 {
  int holdoff[] = {2000, 1000, 400};

  // open device
  _file = CreateFile(device.c_str(),GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL );
  if (_file == INVALID_HANDLE_VALUE)
    throwModemException(stringPrintf(_("opening device '%s'"),
                                     device.c_str()));

  int initTries = 3;
  while (initTries-- > 0)
  {
    // flush all pending output
    FlushFileBuffers(_file);

    // toggle DTR to reset modem
    if (!EscapeCommFunction(_file,CLRDTR))
      throwModemException(_("clearing DTR failed"));
    Sleep(holdoff[initTries]);
    if (!EscapeCommFunction(_file,SETDTR))
      throwModemException(_("setting DTR failed"));

    DCB dcb;
    // get line modes
    if (!GetCommState(_file,&dcb))
      throwModemException(stringPrintf(_("GetCommState device '%s'"),
                                       device.c_str()));

//    if (tcgetattr(_fd, &t) < 0)
//      throwModemException(stringPrintf(_("tcgetattr device '%s'"),
//                                       device.c_str()));

    // set the device to a sane state
    dcb.fBinary = TRUE;
    dcb.BaudRate = lineSpeed;

    // n,8,1
    dcb.fParity = FALSE;
    dcb.Parity = 0;
    dcb.ByteSize = 8;
    dcb.StopBits = 0;

    if (!swHandshake)
    {
      dcb.fInX = FALSE;
      dcb.fOutX = FALSE;
      dcb.fOutxDsrFlow = FALSE;
      dcb.fOutxCtsFlow = FALSE;
    }
    else
    {
      dcb.fInX  = TRUE;
      dcb.fOutX = TRUE;
      dcb.fOutxDsrFlow = FALSE;
      dcb.fOutxCtsFlow = FALSE;
    }
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    
//    t.c_iflag |= IGNPAR;
//    t.c_iflag &= ~(INPCK | ISTRIP | IMAXBEL |
//                   (swHandshake ? CRTSCTS : IXON |  IXOFF)
//                   | IXANY | IGNCR | ICRNL | IMAXBEL | INLCR | IGNBRK);
//    t.c_oflag &= ~(OPOST);
//    // be careful, only touch "known" flags
//    t.c_cflag&= ~(CSIZE | CSTOPB | PARENB | PARODD);
//    t.c_cflag|= CS8 | CREAD | HUPCL |
//      (swHandshake ? IXON |  IXOFF : CRTSCTS) |
//      CLOCAL;
//    t.c_lflag &= ~(ECHO | ECHOE | ECHOPRT | ECHOK | ECHOKE | ECHONL |
//                   ECHOCTL | ISIG | IEXTEN | TOSTOP | FLUSHO | ICANON);
//    t.c_lflag |= NOFLSH;
//
//    t.c_cc[VMIN] = 1;
//    t.c_cc[VTIME] = 0;
//
//    t.c_cc[VSUSP] = 0;

    // write back
    if (!SetCommState(_file,&dcb))
      throwModemException(stringPrintf(_("SetCommState device '%s'"),
                                       device.c_str()));

    Sleep(holdoff[initTries]);

    if (!SetupComm(_file,1024,1024))
      throwModemException(stringPrintf(_("SetupComm device '%s'"),
                                       device.c_str()));
 

    // flush all pending input
    PurgeComm(_file,PURGE_RXABORT|PURGE_RXCLEAR);

    try
    {
      // reset modem
      putLine("ATZ");
      bool foundOK = false;
      int readTries = 5;
      while (readTries-- > 0)
      {
        string s = getLine();
        if (s.find("OK") != string::npos ||
            s.find("CABLE: GSM") != string::npos)
        {
          foundOK = true;
          readTries = 0;           // found OK, exit loop
        }
      }

      if (foundOK)
      {
        // init modem
        readTries = 5;
        // !!! no not declare this in loop, compiler error on Visual C++
        // (without SP and with SP4)
        string s; 
        putLine("AT" + initString);
        do
        {
          s = getLine();
          if (s.find("OK") != string::npos ||
              s.find("CABLE: GSM") != string::npos)
            return;                 // found OK, return
        } while(--readTries);
      }
    }
    catch (GsmException &e)
    {
      if (initTries == 0)
        throw e;
    }
  }
  // no response after 3 tries
  throw GsmException(stringPrintf(_("reset modem failed '%s'"),
                                  device.c_str()), OtherError);
 }
 catch (GsmException &e)
 {
  if ( _file != INVALID_HANDLE_VALUE)
   CloseHandle(_file);  
  throw e;
 }
}

string Win32SerialPort::getLine() throw(GsmException)
{
  string result;
  int c;
  while ((c = readByte()) > 0)
  {
    while (c == CR)
    {
      c = readByte();
    }
    if (c == LF)
      break;
    result += c;
  }

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "<-- " << result << endl;
#endif

  return result;
}

void Win32SerialPort::putLine(string line,
                             bool carriageReturn) throw(GsmException)
{
#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "--> " << line << endl;
#endif

  if (carriageReturn) line += CR;
  // !!! BUG, mantain this pointer isn't corrent, use iterator !!!
  const char *l = line.c_str();
  
  FlushFileBuffers(_file);      // flush all pending input and output

  int timeElapsed = 0;

  DWORD bytesWritten = 0;

  ExceptionSafeOverlapped over;

  DWORD initTime = GetTickCount();
  if (!WriteFile(_file,l,line.length(),&bytesWritten,&over))
  {
    if (GetLastError() != ERROR_IO_PENDING)
    {
      throwModemException(_("writing to TA"));
    }

    while(bytesWritten < (DWORD)line.length())
    {
      if (interrupted())
        throwModemException(_("interrupted when writing to TA"));

      // wait another second
      switch(WaitForSingleObject(over.hEvent,1000))
      {
      case WAIT_TIMEOUT:
        break;
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED:
	// !!! do a infinite loop if (bytesWritten < lenght) ?
        GetOverlappedResult(_file,&over,&bytesWritten,TRUE);
        break;
      case WAIT_FAILED:
        throwModemException(_("writing to TA"));
      }

      timeElapsed = (GetTickCount() - initTime)/1000U;

      // timeout elapsed ?
      if (timeElapsed >= timeoutVal)
      {
        CancelIo(_file);
        throwModemException(_("timeout when writing to TA"));
      }

    }
  }

  return;
/*
  // empty buffer
  SetCommMask(_file,EV_TXEMPTY);
  DWORD dwEvent;
  ResetEvent(over.hEvent);
  if( WaitCommEvent(_file,&dwEvent,&over) )
    return; // already empty

  // check true errors
  if (GetLastError() != ERROR_IO_PENDING)
    throwModemException(_("error comm waiting"));

  while(timeElapsed < timeoutVal)
  {
    if (interrupted())
      throwModemException(_("interrupted when flushing to TA"));

    switch( WaitForSingleObject( over.hEvent, 1000 ) ) 
    {
    case WAIT_TIMEOUT:
      break;

    // successfully flushed
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
      return;

    default:
      throwModemException(_("error waiting"));
    }
    timeElapsed = (GetTickCount() - initTime)/1000U;
  }

  CancelIo(_file);
  throwModemException(_("timeout when writing to TA"));
*/

  // echo CR LF must be removed by higher layer functions in gsm_at because
  // in order to properly handle unsolicited result codes from the ME/TA
}

bool Win32SerialPort::wait(GsmTime timeout) throw(GsmException)
{
  // See differences from UNIX
  // Why do I use Windows ?
  DWORD dwEvent;
  SetCommMask(_file,EV_RXCHAR);
  if (!timeout)
  {
    if( !WaitCommEvent(_file,&dwEvent,NULL) )
      throwModemException(_("error comm waiting"));
    return true;
  }
  
  ExceptionSafeOverlapped over;
  if( !WaitCommEvent(_file,&dwEvent,&over) )
  {
    // check true errors
    if (GetLastError() != ERROR_IO_PENDING)
      throwModemException(_("error comm waiting"));

    switch( WaitForSingleObject( over.hEvent, timeout->tv_sec*1000U+(timeout->tv_usec/1000U) ) ) 
    {
    case WAIT_TIMEOUT:
      CancelIo(_file);
      return false;

    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
      return true;

    default:
      throwModemException(_("error waiting"));
    }
  }

  return true;
}

void Win32SerialPort::setTimeOut(unsigned int timeout)
{
  timeoutVal = timeout;
}

Win32SerialPort::~Win32SerialPort()
{
  if ( _file != INVALID_HANDLE_VALUE)
    CloseHandle(_file);
}

int gsmlib::baudRateStrToSpeed(string baudrate) throw(GsmException)
{
  if (baudrate == "300")
    return 300;
  else if (baudrate == "600")
    return 600;
  else if (baudrate == "1200")
    return 1200;
  else if (baudrate == "2400")
    return 2400;
  else if (baudrate == "4800")
    return 4800;
  else if (baudrate == "9600")
    return 9600;
  else if (baudrate == "19200")
    return 19200;
  else if (baudrate == "38400")
    return 38400;
  else if (baudrate == "57600")
    return 57600;
  else if (baudrate == "115200")
    return 115200;
  else
    throw GsmException(stringPrintf(_("unknown baudrate '%s'"),
                                    baudrate.c_str()), ParameterError);
}
