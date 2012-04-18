// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_at.h
// *
// * Purpose: Utility classes for AT command sequence handling
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.5.1999
// *************************************************************************

#ifndef GSM_AT_H
#define GSM_AT_H

#include <gsmlib/gsm_port.h>
#include <string>
#include <vector>

using namespace std;

namespace gsmlib
{
  // forward declarations
  
  class GsmEvent;
  class MeTa;

  // utiliy class to handle AT sequences

  class GsmAt : public RefBase
  {
  protected:
    MeTa &_meTa;
    Ref<Port> _port;
    GsmEvent *_eventHandler;
    
    // return true if response matches
    bool matchResponse(string answer, string responseToMatch);

    // cut response and normalize
    string cutResponse(string answer, string responseToMatch);

    // parse CME error contained in string and throw MeTaException
    void throwCmeException(string s) throw(GsmException);

  public:
    GsmAt(MeTa &meTa);

    // return MeTa object for this AT object
    MeTa &getMeTa() {return _meTa;}

    // the following sequence functions recognize asynchronous messages
    // from the TA and return the appropriate event

    // send AT command, wait for response response, returns response line
    // without response match
    // if response == "" only an OK is expected
    // white space at beginning or end are removed
    // +CME ERROR or ERROR raises exception (if ignoreErrors == true)
    // additionally, accept empty responses (just an OK)
    //   if acceptEmptyResponse == true
    //   in this case an empty string is returned
    string chat(string atCommand = "",
                string response = "",
                bool ignoreErrors = false,
                bool acceptEmptyResponse = false) throw(GsmException);

    // same as chat() above but also get pdu if expectPdu == true
    string chat(string atCommand,
                string response,
                string &pdu,
                bool ignoreErrors = false,
                bool expectPdu = true,
                bool acceptEmptyResponse = false) throw(GsmException);

    // same as above, but expect several response lines
    vector<string> chatv(string atCommand = "",
                         string response = "",
                         bool ignoreErrors = false)
      throw(GsmException);

    // removes whitespace at beginning and end of string
    string normalize(string s);

    // send pdu (wait for <CR><LF><greater_than><space> and send <CTRL-Z>
    // at the end
    // return text after response
    string sendPdu(string atCommand, string response, string pdu,
		   bool acceptEmptyResponse = false) throw(GsmException);

    // functions from class Port
    string getLine() throw(GsmException);
    void putLine(string line,
                 bool carriageReturn = true) throw(GsmException);
    bool wait(GsmTime timeout) throw(GsmException);
    int readByte() throw(GsmException);

    // set event handler class, return old one
    GsmEvent *setEventHandler(GsmEvent *newHandler);
  };
};

#endif // GSM_AT_H
