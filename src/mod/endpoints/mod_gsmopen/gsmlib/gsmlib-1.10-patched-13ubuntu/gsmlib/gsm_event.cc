// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_event.cc
// *
// * Purpose: Event handler interface
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 7.6.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_event.h>
#include <gsmlib/gsm_parser.h>
#include <gsmlib/gsm_at.h>
#include <gsmlib/gsm_me_ta.h>

using namespace std;
using namespace gsmlib;

// GsmEvent members

void GsmEvent::dispatch(string s, GsmAt &at) throw(GsmException)
{
  SMSMessageType messageType;
  bool indication = false;
  if (s.substr(0, 5) == "+CMT:")
    messageType = NormalSMS;
  else if (s.substr(0, 5) == "+CBM:")
    messageType = CellBroadcastSMS;
  else if (s.substr(0, 5) == "+CDS:")
  {
    // workaround for phones that report CDS when they actually mean CDSI
    indication = at.getMeTa().getCapabilities()._CDSmeansCDSI;
    messageType = StatusReportSMS;
  }
  else if (s.substr(0, 6) == "+CMTI:")
  {
    indication = true;
    messageType = NormalSMS;
  }
  else if (s.substr(0, 6) == "+CBMI:")
  {
    indication = true;
    messageType = CellBroadcastSMS;
  }
  else if (s.substr(0, 6) == "+CDSI:")
  {
    indication = true;
    messageType = StatusReportSMS;
  }
  else if (s.substr(0, 4) == "RING")
  {
    ringIndication();
    return;
  }
  // handling  NO CARRIER
  else if (s.substr(0, 10) == "NO CARRIER")
  {
    noAnswer();
    return;
  }
  
  else if (s.substr(0, 6) == "+CLIP:")
  {
    //    <number>,<type>[,<subaddr>,<satype>[,<alpha>]]
    s = s.substr(6);
    Parser p(s);
    string num = p.parseString();
    if (p.parseComma(true))
    {
      unsigned int numberFormat;
      if ((numberFormat = p.parseInt()) == InternationalNumberFormat)
        num = "+" + num;
      else if (numberFormat != UnknownNumberFormat)
        throw GsmException(stringPrintf(_("unexpected number format %d"),
                                        numberFormat), OtherError);
    }
    string subAddr;
    string alpha;
    if (p.parseComma(true))
    {
      subAddr = p.parseString(true);
      p.parseComma();
      p.parseInt(true);         // FIXME subaddr type ignored

      if (p.parseComma(true))
        alpha = p.parseString(true);
    }
    
    // call the event handler
    callerLineID(num, subAddr, alpha);
    return;
  }
  else
    throw GsmException(stringPrintf(_("unexpected unsolicited event '%s'"),
                                    s.c_str()), OtherError);

  if (indication)
  {
    // handle SMS storage indication
    s = s.substr(6);
    Parser p(s);
    string storeName = p.parseString();
    p.parseComma();
    unsigned int index = p.parseInt();
    SMSReceptionIndication(storeName, index - 1, messageType);
  }
  else
    if (messageType == CellBroadcastSMS)
    {
      // handle CB message
      string pdu = at.getLine();

      CBMessageRef cb = new CBMessage(pdu);

      // call the event handler
      CBReception(cb);
    }
    else
    {
      // handle SMS
      string pdu = at.getLine();
      
      // add missing service centre address if required by ME
      if (! at.getMeTa().getCapabilities()._hasSMSSCAprefix)
      pdu = "00" + pdu;
      
      SMSMessageRef sms = SMSMessage::decode(pdu);
      
      // send acknowledgement if necessary
      if (at.getMeTa().getCapabilities()._sendAck)
        at.chat("+CNMA");
      
      // call the event handler
      SMSReception(sms, messageType);
    }
}

void GsmEvent::callerLineID(string number, string subAddr, string alpha)
{
  // ignore event
}

void GsmEvent::SMSReception(SMSMessageRef newMessage,
                            SMSMessageType messageType)
{
  // ignore event
}

void GsmEvent::CBReception(CBMessageRef newMessage)
{
  // ignore event
}

void GsmEvent::SMSReceptionIndication(string storeName, unsigned int index,
                                      SMSMessageType messageType)
{
  // ignore event
}

void GsmEvent::ringIndication()
{
  // ignore event
}

void GsmEvent::noAnswer()
{
  // ignore event
}
