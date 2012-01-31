// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_unix_port.cc
// *
// * Purpose: UNIX serial port implementation
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 10.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_unix_serial.h>
#include <gsmlib/gsm_util.h>
#include <termios.h>
#include <fcntl.h>
#include <iostream>
#include <strstream>
#include <cassert>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pthread.h>
#include <cstring>

using namespace std;
using namespace gsmlib;

static const int holdoff[] = {2000000, 1000000, 400000};
static const int holdoffArraySize = sizeof(holdoff)/sizeof(int);
  
// alarm handling for socket read/write
// the timerMtx is necessary since several threads cannot use the
// timer indepently of each other

static pthread_mutex_t timerMtx = PTHREAD_MUTEX_INITIALIZER;

// for non-GNU systems, define alarm()
#ifndef HAVE_ALARM
unsigned int alarm(unsigned int seconds)
{
  struct itimerval old, newt;
  newt.it_interval.tv_usec = 0;
  newt.it_interval.tv_sec = 0;
  newt.it_value.tv_usec = 0;
  newt.it_value.tv_sec = (long int)seconds;
  if (setitimer(ITIMER_REAL, &newt, &old) < 0)
    return 0;
  else
    return old.it_value.tv_sec;
}
#endif

// this routine is called in case of a timeout
static void catchAlarm(int)
{
  // do nothing
}

// start timer
static void startTimer()
{
  pthread_mutex_lock(&timerMtx);
  struct sigaction newAction;
  newAction.sa_handler = catchAlarm;
  newAction.sa_flags = 0;
  sigaction(SIGALRM, &newAction, NULL);
  alarm(1);
}

// reset timer
static void stopTimer()
{
  alarm(0);
  sigaction(SIGALRM, NULL, NULL);
  pthread_mutex_unlock(&timerMtx);
}

// UnixSerialPort members

void UnixSerialPort::throwModemException(string message) throw(GsmException)
{
  ostrstream os;
  os << message << " (errno: " << errno << "/" << strerror(errno) << ")"
     << ends;
  char *ss = os.str();
  string s(ss);
  delete[] ss;
  throw GsmException(s, OSError, errno);
}

void UnixSerialPort::putBack(unsigned char c)
{
  assert(_oldChar == -1);
  _oldChar = c;
}

int UnixSerialPort::readByte() throw(GsmException)
{
  if (_oldChar != -1)
  {
    int result = _oldChar;
    _oldChar = -1;
    return result;
  }

  unsigned char c;
  int timeElapsed = 0;
  struct timeval oneSecond;
  bool readDone = false;

  while (! readDone && timeElapsed < _timeoutVal)
  {
    if (interrupted())
      throwModemException(_("interrupted when reading from TA"));

    // setup fd_set data structure for select()
    fd_set fdSet;
    oneSecond.tv_sec = 1;
    oneSecond.tv_usec = 0;
    FD_ZERO(&fdSet);
    FD_SET(_fd, &fdSet);

    switch (select(FD_SETSIZE, &fdSet, NULL, NULL, &oneSecond))
    {
    case 1:
    {
      int res = read(_fd, &c, 1);
      if (res != 1)
        throwModemException(_("end of file when reading from TA"));
      else
        readDone = true;
      break;
    }
    case 0:
      ++timeElapsed;
      break;
    default:
      if (errno != EINTR)
        throwModemException(_("reading from TA"));
      break;
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

UnixSerialPort::UnixSerialPort(string device, speed_t lineSpeed,
                               string initString, bool swHandshake)
  throw(GsmException) :
  _oldChar(-1), _timeoutVal(TIMEOUT_SECS)
{
  struct termios t;

  // open device
  _fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (_fd == -1)
    throwModemException(stringPrintf(_("opening device '%s'"),
                                     device.c_str()));

  // switch off non-blocking mode
  int fdFlags;
  if ((fdFlags = fcntl(_fd, F_GETFL)) == -1) {
    close(_fd);
    throwModemException(_("getting file status flags failed"));
  }
  fdFlags &= ~O_NONBLOCK;
  if (fcntl(_fd, F_SETFL, fdFlags) == -1) {
    close(_fd);
    throwModemException(_("switching of non-blocking mode failed"));
  }

  long int saveTimeoutVal = _timeoutVal;
  _timeoutVal = 3;
  int initTries = holdoffArraySize;
  while (initTries-- > 0)
  {
    // flush all pending output
    tcflush(_fd, TCOFLUSH);

    // toggle DTR to reset modem
    int mctl = TIOCM_DTR;
    if (ioctl(_fd, TIOCMBIC, &mctl) < 0) {
      close(_fd);
      throwModemException(_("clearing DTR failed"));
    }
    // the waiting time for DTR toggling is increased with each loop
    usleep(holdoff[initTries]);
    if (ioctl(_fd, TIOCMBIS, &mctl) < 0) {
      close(_fd);
      throwModemException(_("setting DTR failed"));
    }
    // get line modes
    if (tcgetattr(_fd, &t) < 0) {
      close(_fd);
      throwModemException(stringPrintf(_("tcgetattr device '%s'"),
                                       device.c_str()));
    }

    // set line speed
    cfsetispeed(&t, lineSpeed);
    cfsetospeed(&t, lineSpeed);

    // set the device to a sane state
    t.c_iflag |= IGNPAR | (swHandshake ? IXON | IXOFF : 0);
    t.c_iflag &= ~(INPCK | ISTRIP | IMAXBEL |
                   (swHandshake ? 0 : IXON |  IXOFF)
                   | IXANY | IGNCR | ICRNL | IMAXBEL | INLCR | IGNBRK);
    t.c_oflag &= ~(OPOST);
    // be careful, only touch "known" flags
    t.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD |
                  (swHandshake ? CRTSCTS : 0 ));
    t.c_cflag |= CS8 | CREAD | HUPCL | (swHandshake ? 0 : CRTSCTS) | CLOCAL;
    t.c_lflag &= ~(ECHO | ECHOE | ECHOPRT | ECHOK | ECHOKE | ECHONL |
                   ECHOCTL | ISIG | IEXTEN | TOSTOP | FLUSHO | ICANON);
    t.c_lflag |= NOFLSH;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;

    t.c_cc[VSUSP] = 0;

    // write back
    if(tcsetattr (_fd, TCSANOW, &t) < 0) {
      close(_fd);
      throwModemException(stringPrintf(_("tcsetattr device '%s'"),
                                       device.c_str()));
    }
    // the waiting time for writing to the ME/TA is increased with each loop
    usleep(holdoff[initTries]);

    // flush all pending input
    tcflush(_fd, TCIFLUSH);

    try
    {
      // reset modem
      putLine("ATZ");
      bool foundOK = false;
      int readTries = 5;
      while (readTries-- > 0)
      {
        // for the first call getLine() waits only 3 seconds
        // because of _timeoutVal = 3
        string s = getLine();
        if (s.find("OK") != string::npos ||
            s.find("CABLE: GSM") != string::npos)
        {
          foundOK = true;
          readTries = 0;        // found OK, exit loop
        }
        else if (s.find("ERROR") != string::npos)
          readTries = 0;        // error, exit loop
      }

      // set getLine/putLine timeout back to old value
      _timeoutVal = saveTimeoutVal;

      if (foundOK)
      {
        // init modem
        readTries = 5;
        putLine("AT" + initString);
        while (readTries-- > 0)
        {
          string s = getLine();
          if (s.find("OK") != string::npos ||
              s.find("CABLE: GSM") != string::npos)
            return;                 // found OK, return
        }
      }
    }
    catch (GsmException &e)
    {
      _timeoutVal = saveTimeoutVal;
      if (initTries == 0) {
        close(_fd);
        throw e;
      }
    }
  }
  // no response after 3 tries
  close(_fd);
  throw GsmException(stringPrintf(_("reset modem failed '%s'"),
                                  device.c_str()), OtherError);
}

string UnixSerialPort::getLine() throw(GsmException)
{
  string result;
  int c;
  while ((c = readByte()) >= 0)
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

void UnixSerialPort::putLine(string line,
                             bool carriageReturn) throw(GsmException)
{
#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "--> " << line << endl;
#endif

  if (carriageReturn) line += CR;
  const char *l = line.c_str();
  
  int timeElapsed = 0;
  struct timeval oneSecond;

  ssize_t bytesWritten = 0;
  while (bytesWritten < (ssize_t)line.length() && timeElapsed < _timeoutVal)
  {
    if (interrupted())
      throwModemException(_("interrupted when writing to TA"));

    // setup fd_set data structure for select()
    fd_set fdSet;
    oneSecond.tv_sec = 1;
    oneSecond.tv_usec = 0;
    FD_ZERO(&fdSet);
    FD_SET(_fd, &fdSet);

    switch (select(FD_SETSIZE, NULL, &fdSet, NULL, &oneSecond))
    {
    case 1:
    {
      ssize_t bw = write(_fd, l + bytesWritten, line.length() - bytesWritten);
      if (bw < 0)
        throwModemException(_("writing to TA"));
      bytesWritten += bw;
      break;
    }
    case 0:
      ++timeElapsed;
      break;
    default:
      if (errno != EINTR)
        throwModemException(_("writing to TA"));
      break;
    }
  }
  
  while (timeElapsed < _timeoutVal)
  {
    if (interrupted())
      throwModemException(_("interrupted when writing to TA"));
    startTimer();
    int res = tcdrain(_fd);     // wait for output to be read by TA
    stopTimer();
    if (res == 0)
      break;
    else
    {
      assert(errno == EINTR);
      ++timeElapsed;
    }
  }
  if (timeElapsed >= _timeoutVal)
    throwModemException(_("timeout when writing to TA"));

  // echo CR LF must be removed by higher layer functions in gsm_at because
  // in order to properly handle unsolicited result codes from the ME/TA
}

bool UnixSerialPort::wait(GsmTime timeout) throw(GsmException)
{
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(_fd, &fds);
  return select(FD_SETSIZE, &fds, NULL, NULL, timeout) != 0;
}

// set timeout for read or write in seconds.
void UnixSerialPort::setTimeOut(unsigned int timeout)
{
  _timeoutVal = timeout;
}

UnixSerialPort::~UnixSerialPort()
{
  if (_fd != -1)
    close(_fd);
}

speed_t gsmlib::baudRateStrToSpeed(string baudrate) throw(GsmException)
{
  if (baudrate == "300")
    return B300;
  else if (baudrate == "600")
    return B600;
  else if (baudrate == "1200")
    return B1200;
  else if (baudrate == "2400")
    return B2400;
  else if (baudrate == "4800")
    return B4800;
  else if (baudrate == "9600")
    return B9600;
  else if (baudrate == "19200")
    return B19200;
  else if (baudrate == "38400")
    return B38400;
#ifdef B57600
  else if (baudrate == "57600")
    return B57600;
#endif
#ifdef B115200
  else if (baudrate == "115200")
    return B115200;
#endif
#ifdef B230400
  else if (baudrate == "230400")
    return B230400;
#endif
#ifdef B460800
  else if (baudrate == "460800")
    return B460800;
#endif
  else
    throw GsmException(stringPrintf(_("unknown baudrate '%s'"),
                                    baudrate.c_str()), ParameterError);
}
