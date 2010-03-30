// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_util.h
// *
// * Purpose: Various utilities
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sysdep.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <iostream>
#include <strstream>
#include <ctype.h>
#include <errno.h>
#if !defined(HAVE_CONFIG_H) || defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if !defined(HAVE_CONFIG_H) || defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif
#include <stdarg.h>
#ifdef HAVE_VSNPRINTF
// switch on vsnprintf() prototype in stdio.h
#define __USE_GNU
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <stdio.h>
#include <sys/stat.h>

using namespace std;
using namespace gsmlib;

// Latin-1 undefined character (code 172 (Latin-1 boolean not, "¬"))
const int NOP = 172;

// GSM undefined character (code 16 (GSM Delta))
const int GSM_NOP = 16;

// conversion tables, Latin1 to GSM and GSM to Latin1

static unsigned char gsmToLatin1Table[] =
{
  //  0 '@', '£', '$', '¥', 'è', 'é', 'ù', 'ì', 
        '@', 163, '$', 165, 232, 233, 249, 236,
  //  8 'ò', 'Ç',  LF, 'Ø', 'ø',  CR, 'Å', 'å', 
        242, 199,  10, 216, 248,  13, 197, 229,
  // 16 '¬', '_', '¬', '¬', '¬', '¬', '¬', '¬',
        NOP, '_', NOP, NOP, NOP, NOP, NOP, NOP, 
  // 24 '¬', '¬', '¬', '¬', 'Æ', 'æ', 'ß', 'É',
        NOP, NOP, NOP, NOP, 198, 230, 223, 201, 
  // 32 ' ', '!', '"', '#', '¤', '%', '&', ''',
        ' ', '!', '"', '#', 164, '%', '&', '\'',
  // 40 '(', ')', '*', '+', ',', '-', '.', '/',
        '(', ')', '*', '+', ',', '-', '.', '/',
  // 48 '0', '1', '2', '3', '4', '5', '6', '7',
         '0', '1', '2', '3', '4', '5', '6', '7',
  // 56 '8', '9', ':', ';', '<', '=', '>', '?', 
        '8', '9', ':', ';', '<', '=', '>', '?', 
  // 64 '¡', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 
        161, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 
  // 72 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
        'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  // 80 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
         'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
  // 88 'X', 'Y', 'Z', 'Ä', 'Ö', 'Ñ', 'Ü', '§', 
        'X', 'Y', 'Z', 196, 214, 209, 220, 167,
  // 96 '¿', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
        191, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
  // 104 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
         'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
  // 112 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
         'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
  // 120 'x', 'y', 'z', 'ä', 'ö', 'ñ', 'ü', 'à', 
         'x', 'y', 'z', 228, 246, 241, 252, 224
};

static unsigned char latin1ToGsmTable[256];

static class Latin1ToGsmTableInit
{
public:
  Latin1ToGsmTableInit()
  {
    memset((void*)latin1ToGsmTable, GSM_NOP, 256);
    for (int i = 0; i < 128; i++)
      if (gsmToLatin1Table[i] != NOP)
        latin1ToGsmTable[gsmToLatin1Table[i]] = i;
  }
} latin1ToGsmTableInit;

string gsmlib::gsmToLatin1(string s)
{
  string result(s.length(), 0);
  for (string::size_type i = 0; i < s.length(); i++)
    result[i] = (unsigned char)s[i] > 127 ? NOP : gsmToLatin1Table[s[i]];
  return result;
}

string gsmlib::latin1ToGsm(string s)
{
  string result(s.length(), 0);
  for (string::size_type i = 0; i < s.length(); i++)
    result[i] = latin1ToGsmTable[(unsigned char)s[i]];
  return result;
}

static unsigned char byteToHex[] =
{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
 'A', 'B', 'C', 'D', 'E', 'F'};

string gsmlib::bufToHex(const unsigned char *buf, unsigned long length)
{
  const unsigned char *bb = buf;
  string result;
  result.reserve(length * 2);

  for (unsigned long i = 0; i < length; ++i)
  {
    result += byteToHex[*bb >> 4];
    result += byteToHex[*bb++ & 0xf];
  }
  return result;
}

bool gsmlib::hexToBuf(const string &hexString, unsigned char *buf)
{
  if (hexString.length() % 2 != 0)
    return false;

  unsigned char *bb = buf;
  for (unsigned int i = 0; i < hexString.length(); i += 2)
  {
    unsigned char c = hexString[i];
    if (! isdigit(c) && ! ('a' <= c && c <= 'f') && ! ('A' <= c && c <= 'F'))
      return false;
    *bb = (isdigit(c) ? c - '0' :
           ((('a' <= c && c <= 'f') ? c - 'a' : c - 'A')) + 10) << 4;
    c = hexString[i + 1];
    if (! isdigit(c) && ! ('a' <= c && c <= 'f') && ! ('A' <= c && c <= 'F'))
      return false;
    *bb++ |= isdigit(c) ? c - '0' :
      ((('a' <= c && c <= 'f') ? c - 'a' : c - 'A') + 10);
  }
  return true;
}

string gsmlib::intToStr(int i)
{
  ostrstream os;
  os << i << ends;
  char *ss = os.str();
  string s(ss);
  delete[] ss;
  return s;
}

string gsmlib::removeWhiteSpace(string s)
{
  string result;
  for (unsigned int i = 0; i < s.length(); ++i)
    if (! isspace(s[i]))
      result += s[i];
  return result;
}

#ifdef WIN32

// helper routine, find out whether filename starts with "COM"
static bool isCom(string filename)
{
  filename = removeWhiteSpace(lowercase(filename));
  // remove UNC begin
  if ( filename.compare(0, 4, "\\\\.\\") == 0 )
    filename.erase(0, 4);
  return filename.length() < 3 || filename.substr(0, 3) == "com";
}
#endif

bool gsmlib::isFile(string filename)
{
#ifdef WIN32
  // stat does not work reliably under Win32 to indicate devices
  if (isCom(filename))
    return false;
#endif

  struct stat statBuf;
  int retries = 0;

  while (retries < 10)
  {
    if (stat(filename.c_str(), &statBuf) != 0)
      throw GsmException(
        stringPrintf(_("error when calling stat('%s') (errno: %d/%s)"), 
                     filename.c_str(), errno, strerror(errno)),
        OSError);
    
#ifndef WIN32
    if (S_ISLNK(statBuf.st_mode))
    {
      int size = 100;
      while (1)
      {
        char *buffer = (char*)malloc(size);
        int nchars = readlink(filename.c_str(), buffer, size);
        if (nchars < size)
        {
          filename.assign(buffer, nchars);
          free(buffer);
          break;
        }
        free(buffer);
        size *= 2;
      }
      ++retries;
    }
    else if (S_ISCHR(statBuf.st_mode))
      return false;
    else 
#endif
    if (S_ISREG(statBuf.st_mode))
      return true;
    else
      throw GsmException(
        stringPrintf(_("file '%s' is neither file nor character device"),
                     filename.c_str()),
        ParameterError);
  }
  throw GsmException(_("maxmimum number of symbolic links exceeded"),
                     ParameterError);
}

void gsmlib::renameToBackupFile(string filename) throw(GsmException)
{
  string backupFilename = filename + "~";
  unlink(backupFilename.c_str());
  if (rename(filename.c_str(), backupFilename.c_str()) < 0)
    throw GsmException(
      stringPrintf(_("error renaming '%s' to '%s'"),
                   filename.c_str(), backupFilename.c_str()),
      OSError, errno);
}

// NoCopy members

#ifndef NDEBUG

NoCopy::NoCopy(NoCopy &n)
{
  cerr << "ABORT: NoCopy copy constructor used" << endl;
  abort();
}

NoCopy &NoCopy::operator=(NoCopy &n)
{
  cerr << "ABORT: NoCopy::operator= used" << endl;
  abort();
  return n;
}

#endif // NDEBUG

string gsmlib::lowercase(string s)
{
  string result;
  for (unsigned int i = 0; i < s.length(); ++i)
    result += tolower(s[i]);
  return result;
}

int gsmlib::checkNumber(string s) throw(GsmException)
{
  for (unsigned int i = 0; i < s.length(); ++i)
    if (! isdigit(s[i]))
      throw GsmException(stringPrintf(_("expected number, got '%s'"),
                                      s.c_str()), ParameterError);
  int result;
  istrstream is(s.c_str());
  is >> result;
  return result;
}

#ifdef HAVE_VSNPRINTF
string gsmlib::stringPrintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int size = 1024;
  while (1)
  {
    char *buf = (char*)alloca(sizeof(char) * size);
    int nchars = vsnprintf(buf, size, format, args);
    if (nchars < size)
    {
      va_end(args);
      return string(buf, nchars);
    }
    size *= 2;
  }
  return "";
}

#else
char gsmlib::__s[20000];        // buffer for the replacement macro
#endif // HAVE_VSNPRINTF

#ifndef NDEBUG
int gsmlib::debugLevel()
{
  char *s = getenv("GSMLIB_DEBUG");
  if (s == NULL) return 0;
  return checkNumber(s);
}
#endif

// interrupt interface

namespace gsmlib
{
  static InterruptBase *interruptObject = NULL;
}

void gsmlib::setInterruptObject(InterruptBase *intObject)
{
  interruptObject = intObject;
}
  
bool gsmlib::interrupted()
{
  return interruptObject != NULL && interruptObject->interrupted();
}

void gsmlib::checkTextAndTelephone(string text, string telephone)
  throw(GsmException)
{
  if (text.find('"') != string::npos)
    throw GsmException(
      stringPrintf(_("text '%s' contains illegal character '\"'"),
                   text.c_str()),
      ParameterError);

  for (unsigned int i = 0; i < telephone.length(); ++i)
    if (! isdigit(telephone[i]) && ! (telephone[i] == '+') &&
        ! (telephone[i] == '*') && ! (telephone[i] == '#') &&
        ! (telephone[i] == 'p') && ! (telephone[i] == 'w') &&
        ! (telephone[i] == 'P') && ! (telephone[i] == 'W'))
      throw GsmException(
        stringPrintf(_("illegal character in telephone number '%s'"),
                     telephone.c_str()), ParameterError);
}

// progress interface

namespace gsmlib
{
  static ProgressBase *progressObject = NULL;
}

void gsmlib::setProgressObject(ProgressBase *progObject)
{
  progressObject = progObject;
}

void gsmlib::reportProgress(int part, int total)
{
  if (progressObject != NULL)
    progressObject->reportProgress(part, total);
}
