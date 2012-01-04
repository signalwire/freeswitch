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

#ifndef GSM_UTIL_H
#define GSM_UTIL_H

#include <string>
#include <vector>
#include <gsmlib/gsm_error.h>
#ifndef WIN32
#include <sys/time.h>
#endif
#include <stdio.h>

using namespace std;

namespace gsmlib
{
  // time type
  typedef struct timeval *GsmTime;

  // some constants
  const char CR = 13;             // ASCII carriage return
  const char LF = 10;             // ASCII line feed

  // common number formats
  const unsigned int UnknownNumberFormat = 129;
  const unsigned int InternationalNumberFormat = 145;

  // convert gsm to Latin-1
  // characters that have no counterpart in Latin-1 are converted to
  // code 172 (Latin-1 boolean not, "¬")
  string gsmToLatin1(string s);

  // convert Latin-1 to gsm
  // characters that have no counterpart in GSM are converted to
  // code 16 (GSM Delta)
  string latin1ToGsm(string s);

  // convert byte buffer of length to hexadecimal string
  string bufToHex(const unsigned char *buf, unsigned long length);

  // convert hexString to byte buffer, return false if no hexString
  bool hexToBuf(const string &hexString, unsigned char *buf);

  // indicate that a value is not set
  const int NOT_SET = -1;

  // An integer range
  struct IntRange
  {
    int _high, _low;

    IntRange() : _high(NOT_SET), _low(NOT_SET) {}
  };

  // A valid integer range for a given parameter
  struct ParameterRange
  {
    string _parameter;
    IntRange _range;
  };

  // *** general-purpose pointer wrapper with reference counting

  class RefBase
  {
  private:
    int _refCount;

  public:
    RefBase() : _refCount(0) {}
    int ref() {return _refCount++;}
    int unref() {return --_refCount;}
    int refCount() const {return _refCount;}
  };

  template <class T>
    class Ref
    {
    private:
      T *_rep;
    public:
      T *operator->() const {return _rep;}
      T &operator()() {return *_rep;}
      T *getptr() {return _rep;}
      bool isnull() const {return _rep == (T*)NULL;}
      Ref() : _rep((T*)NULL) {}
      Ref(T *pp) : _rep(pp) {if (pp != (T*)NULL) pp->ref();}
      Ref(const Ref &r);
      Ref &operator=(const Ref &r);
      ~Ref();
      bool operator==(const Ref &r) const
        {
          return _rep == r._rep;
        }
    };

  template <class T>
    Ref<T>::Ref(const Ref<T> &r) : _rep(r._rep)
    {
      if (_rep != (T*)NULL) _rep->ref();
    }

  template <class T>
    Ref<T> &Ref<T>::operator=(const Ref<T> &r)
    {
      if (r._rep != (T*)NULL) r._rep->ref();
      if (_rep != (T*)NULL && _rep->unref() == 0) delete _rep;
      _rep = r._rep;
      return *this;
    }

  template <class T>
    Ref<T>::~Ref()
    {
      if (_rep != (T*)NULL && _rep->unref() == 0) delete _rep;
    }

  // utility function return string given an int
  string intToStr(int i);

  // remove white space from the string
  string removeWhiteSpace(string s);

  // return true if bit is set in vector<bool>
  inline bool isSet(vector<bool> &b, unsigned int bit)
    {
      return b.size() > bit && b[bit];
    }

  // return true if filename refers to a file
  // throws exception if filename is neither file nor device
  bool isFile(string filename);

  // make backup file adequate for this operating system
  void renameToBackupFile(string filename) throw(GsmException);

  // Base class for class for which copying is not allow
  // only used for debugging

  class NoCopy
  {
  public:
    NoCopy() {}

#ifndef NDEBUG
    NoCopy(NoCopy &n);

    NoCopy &operator=(NoCopy &n);
#endif
  };

  // convert string to lower case
  string lowercase(string s);

  // convert string to number and check for all digits
  int checkNumber(string s) throw(GsmException);

  // like printf, but return C++ string
#ifdef HAVE_VSNPRINTF
  string stringPrintf(const char *format, ...)
#if	__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
     __attribute__((format (printf, 1, 2)))
#endif
    ;
#else
  // WARNING: This replacement code is
  // - not threadsafe
  // - subject to buffer overruns
#define stringPrintf(format, args...)                   \
        (sprintf(__s, format, ## args), string(__s))

  extern char __s[];
#endif // HAVE_VSNPRINTF

  // return debug level
#ifndef NDEBUG
  extern int debugLevel();
#endif

  // interface for interrupting gsmlib activity

  class InterruptBase
  {
  public:
    // this member should return true if gsmlib is to be interrupted
    virtual bool interrupted() = 0;
  };

  // set global interrupt object
  extern void setInterruptObject(InterruptBase *intObject);
  
  // return true if interrupted
  extern bool interrupted();

  // interface for reporting progress
  
  class ProgressBase
  {
  public:
    // override this to receive progress reports
    virtual void reportProgress(int part, int total) = 0;
  };

  // set global progress object
  extern void setProgressObject(ProgressBase *progObject);

  // report progress (part/total * 100 is meant to be the percentage)
  // this function is called by
  // - GsmAt::chatv() without arguments, used by Phonebook::Phonebook()
  // - Phonebook::Phonebook()
  // - SortedPhonebook::SortedPhonebook()
  // - SortedSMSStore::SortedSMSStore()
  extern void reportProgress(int part = -1, int total = -1);

  // check for valid text and telephone number
  // throw exception if error
  extern void checkTextAndTelephone(string text, string telephone)
    throw(GsmException);
};

#endif // GSM_UTIL_H
