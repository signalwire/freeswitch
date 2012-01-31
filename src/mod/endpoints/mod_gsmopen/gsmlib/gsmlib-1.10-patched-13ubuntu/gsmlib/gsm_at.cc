// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_at.cc
// *
// * Purpose: Utility classes for AT command sequence handling
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_at.h>
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_error.h>
#include <gsmlib/gsm_event.h>
#include <gsmlib/gsm_me_ta.h>
#include <ctype.h>
#include <strstream>

using namespace std;
using namespace gsmlib;

// GsmAt members

bool GsmAt::matchResponse(string answer, string responseToMatch)
{
  if (answer.substr(0, responseToMatch.length()) == responseToMatch)
    return true;
  else
    // some TAs omit the ':' at the end of the response
    if (_meTa.getCapabilities()._omitsColon &&
        responseToMatch[responseToMatch.length() - 1] == ':' &&
        answer.substr(0, responseToMatch.length() - 1) == 
        responseToMatch.substr(0, responseToMatch.length() - 1))
      return true;
  return false;
}

string GsmAt::cutResponse(string answer, string responseToMatch)
{
  if (answer.substr(0, responseToMatch.length()) == responseToMatch)
    return normalize(answer.substr(responseToMatch.length(),
                                   answer.length() -
                                   responseToMatch.length()));
  else
    // some TAs omit the ':' at the end of the response
    if (_meTa.getCapabilities()._omitsColon &&
        responseToMatch[responseToMatch.length() - 1] == ':' &&
        answer.substr(0, responseToMatch.length() - 1) == 
        responseToMatch.substr(0, responseToMatch.length() - 1))
      return normalize(answer.substr(responseToMatch.length() - 1,
                                     answer.length() -
                                     responseToMatch.length() + 1));
  assert(0);
  return "";
}

void GsmAt::throwCmeException(string s) throw(GsmException)
{
  if (matchResponse(s, "ERROR"))
    throw GsmException(_("unspecified ME/TA error"), ChatError);

  bool meError = matchResponse(s, "+CME ERROR:");
  if (meError)
    s = cutResponse(s, "+CME ERROR:");
  else
    s = cutResponse(s, "+CMS ERROR:");
  istrstream is(s.c_str());
  int error;
  is >> error;
  throw GsmException(_("ME/TA error '") +
                     (meError ? getMEErrorText(error) :
                      getSMSErrorText(error)) +
                     "' " +
                     stringPrintf(_("(code %s)"), s.c_str()),
                     ChatError, error);
}

GsmAt::GsmAt(MeTa &meTa) :
  _meTa(meTa), _port(meTa.getPort()), _eventHandler(NULL)
{
}

string GsmAt::chat(string atCommand, string response,
                   bool ignoreErrors, bool acceptEmptyResponse)
  throw(GsmException)
{
  string dummy;
  return chat(atCommand, response, dummy, ignoreErrors, false,
              acceptEmptyResponse);
}

string GsmAt::chat(string atCommand, string response, string &pdu,
                   bool ignoreErrors, bool expectPdu,
                   bool acceptEmptyResponse) throw(GsmException)
{
  string s;
  bool gotOk = false;           // special handling for empty SMS entries

  // send AT command
  putLine("AT" + atCommand);
  // and gobble up CR/LF (and possibly echoed characters if echo can't be
  // switched off)
  // Also, some mobiles (e.g., Sony Ericsson K800i) respond to commands
  // like "at+cmgf=0" with "+CMGF: 0" on success as well as the "OK"
  // status -- so gobble that (but not if that sort of response was expected)
  // FIXME: this is a gross hack, should be done via capabilities or sth
  #include <string>
  string::size_type loc = atCommand.find( "=", 1 );
  string expect;
  if (loc != string::npos) {
      expect = atCommand;
      expect.replace(loc, 1, " ");
      expect.insert(loc, ":");
  } else {
      expect = "";
  }
  do
  {
    s = normalize(getLine());
  }
  while (s.length() == 0 || s == "AT" + atCommand || 
         ((response.length() == 0 || !matchResponse(s, response)) &&
          (expect.length() > 0 && matchResponse(s, expect))));

  // handle errors
  if (matchResponse(s, "+CME ERROR:") || matchResponse(s, "+CMS ERROR:"))
    if (ignoreErrors)
      return "";
    else
      throwCmeException(s);
  if (matchResponse(s, "ERROR"))
    if (ignoreErrors)
      return "";
    else
      throw GsmException(_("ME/TA error '<unspecified>' (code not known)"), 
                         ChatError, -1);

  // return if response is "OK" and caller says this is OK
  if (acceptEmptyResponse && s == "OK")
    return "";

  // handle PDU if one is expected
  if (expectPdu)
  {
    string ps;
    do
    {
      ps = normalize(getLine());
    }
    while (ps.length() == 0 && ps != "OK");
    if (ps == "OK")
      gotOk = true;
    else
    {
      pdu = ps;
      // remove trailing zero added by some devices (e.g. Falcom A2-1)
      if (pdu.length() > 0 && pdu[pdu.length() - 1] == 0)
        pdu.erase(pdu.length() - 1);
    }
  }

  // handle expected response
  if (response.length() == 0)   // no response expected
  {
    if (s == "OK") return "";
    // else fall through to error
  }
  else
  {
    string result;
    // some TA/TEs don't prefix their response with the response string
    // as proscribed by the standard: just handle either case
    if (matchResponse(s, response))
      result = cutResponse(s, response);
    else
      result = s;

    if (gotOk)
      return result;
    else
    {
      // get the final "OK"
      do
      {
        s = normalize(getLine());
      }
      while (s.length() == 0);

      if (s == "OK") return result;
      // else fall through to error
    }
  }
  throw GsmException(
    stringPrintf(_("unexpected response '%s' when sending 'AT%s'"),
                 s.c_str(), atCommand.c_str()),
    ChatError);
}

vector<string> GsmAt::chatv(string atCommand, string response,
                            bool ignoreErrors) throw(GsmException)
{
  string s;
  vector<string> result;

  // send AT command
  putLine("AT" + atCommand);
  // and gobble up CR/LF (and possibly echoed characters if echo can't be
  // switched off)
  do
  {
    s = normalize(getLine());
  }
  while (s.length() == 0 || s == "AT" + atCommand);

  // handle errors
  if (matchResponse(s, "+CME ERROR:") || matchResponse(s, "+CMS ERROR:"))
    if (ignoreErrors)
      return result;
    else
      throwCmeException(s);
  if (matchResponse(s, "ERROR"))
    if (ignoreErrors)
      return result;
    else
      throw GsmException(_("ME/TA error '<unspecified>' (code not known)"), 
                         ChatError, -1);

  // push all lines that are not empty
  // cut response prefix if it is there
  // stop when an OK line is read
  while (1)
  {
    if (s == "OK")
      return result;
    // some TA/TEs don't prefix their response with the response string
    // as proscribed by the standard: just handle either case
    if (response.length() != 0 && matchResponse(s, response))
      result.push_back(cutResponse(s, response));
    else
      result.push_back(s);
    // get next line
    do
    {
      s = normalize(getLine());
    }
    while (s.length() == 0);
    reportProgress();
  }

  // never reached
  assert(0);
  return result;
}

string GsmAt::normalize(string s)
{
  size_t start = 0, end = s.length();
  bool changed = true;

  while (start < end && changed)
  {
    changed = false;
    if (isspace(s[start]))
    {
      ++start;
      changed = true;
    }
    else
      if (isspace(s[end - 1]))
      {
        --end;
        changed = true;
      }
  }
  return s.substr(start, end - start);
}

string GsmAt::sendPdu(string atCommand, string response, string pdu,
                      bool acceptEmptyResponse) throw(GsmException)
{
  string s;
  bool errorCondition;
  bool retry = false;
  int tries = 5;                // How many error conditions do we accept

  int c;
  do
  {
    errorCondition = false;
    putLine("AT" + atCommand);
    do
    {
      retry = false;
      try
      {
        do
        {
          // read first of two bytes "> "
          c = readByte();
	}
        // there have been reports that some phones give spurious CRs
	// LF separates CDSI messages if there are more than one
	while (c == CR || c == LF);
      }
      catch (GsmException &e)
      {
        c = '-';
        errorCondition = true;  // TA does not expect PDU anymore, retry
      }

      if (c == '+' || c == 'E') // error or unsolicited result code
      {
        _port->putBack(c);
        s = normalize(getLine());
        errorCondition = (s != "");
      
        retry = ! errorCondition;
      }
    }
    while (retry);
  }
  while (errorCondition && tries--);

  if (! errorCondition)
  {
    
    if (c != '>' || readByte() != ' ')
      throw GsmException(_("unexpected character in PDU handshake"),
                         ChatError);
    
    putLine(pdu + "\032", false); // write pdu followed by CTRL-Z

    // some phones (Ericcson T68, T39) send spurious zero characters after
    // accepting the PDU
    c = readByte();
    if (c != 0)
      _port->putBack(c);

    // loop while empty lines (maybe with a zero, Ericsson T39m)
    // or an echo of the pdu (with or without CTRL-Z)
    // is read
    do
    {
      s = normalize(getLine());
    }
    while (s.length() == 0 || s == pdu || s == (pdu + "\032") ||
           (s.length() == 1 && s[0] == 0));
  }

  // handle errors
  if (matchResponse(s, "+CME ERROR:") || matchResponse(s, "+CMS ERROR:"))
    throwCmeException(s);
  if (matchResponse(s, "ERROR"))
    throw GsmException(_("ME/TA error '<unspecified>' (code not known)"), 
                       ChatError, -1);

  // return if response is "OK" and caller says this is OK
  if (acceptEmptyResponse && s == "OK")
    return "";

  if (matchResponse(s, response))
  {
    string result = cutResponse(s, response);
    // get the final "OK"
    do
    {
      s = normalize(getLine());
    }
    while (s.length() == 0);

    if (s == "OK") return result;
    // else fall through to error
  }
  throw GsmException(
    stringPrintf(_("unexpected response '%s' when sending 'AT%s'"),
                 s.c_str(), atCommand.c_str()),
    ChatError);
}

string GsmAt::getLine() throw(GsmException)
{
  if (_eventHandler == (GsmEvent*)NULL)
    return _port->getLine();
  else
  {
    bool eventOccurred;
    string result;
    do
    {
      eventOccurred = false;
      result = _port->getLine();
      string s = normalize(result);
      if (matchResponse(s, "+CMT:") ||
          matchResponse(s, "+CBM:") ||
          matchResponse(s, "+CDS:") ||
          matchResponse(s, "+CMTI:") ||
          matchResponse(s, "+CBMI:") ||
          matchResponse(s, "+CDSI:") ||
          matchResponse(s, "RING") ||
	  matchResponse(s, "NO CARRIER") ||
          // hack: the +CLIP? sequence returns +CLIP: n,m
          // which is NOT an unsolicited result code
          (matchResponse(s, "+CLIP:") && s.length() > 10))
      {
        _eventHandler->dispatch(s, *this);
        eventOccurred = true;
      }
    }
    while (eventOccurred);
    return result;
  }
}

void GsmAt::putLine(string line,
                    bool carriageReturn) throw(GsmException)
{
  _port->putLine(line, carriageReturn);
  // remove empty echo line
  if (carriageReturn)
    getLine();
}

bool GsmAt::wait(GsmTime timeout) throw(GsmException)
{
  return _port->wait(timeout);
}

int GsmAt::readByte() throw(GsmException)
{
  return _port->readByte();
}

GsmEvent *GsmAt::setEventHandler(GsmEvent *newHandler)
{
  GsmEvent *result = _eventHandler;
  _eventHandler = newHandler;
  return result;
}
