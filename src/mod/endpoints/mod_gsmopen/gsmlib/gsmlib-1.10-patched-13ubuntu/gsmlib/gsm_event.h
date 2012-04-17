// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_event.h
// *
// * Purpose: Event handler interface
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 7.6.1999
// *************************************************************************

#ifndef GSM_EVENT_H
#define GSM_EVENT_H

#include <gsmlib/gsm_sms.h>
#include <gsmlib/gsm_cb.h>

using namespace std;

namespace gsmlib
{
  // forward declarations

  class GsmAt;

  // event handler interface

  class GsmEvent
  {
  private:
    // dispatch CMT/CBR/CDS/CLIP etc.
    void dispatch(string s, GsmAt &at) throw(GsmException);

  public:
    // for SMSReception, type of SMS
    enum SMSMessageType {NormalSMS, CellBroadcastSMS, StatusReportSMS};

    // caller line identification presentation
    // only called if setCLIPEvent(true) is set
    virtual void callerLineID(string number, string subAddr, string alpha);

    // called if the string NO CARRIER is read
    virtual void noAnswer();

    // SMS reception
    // only called if setSMSReceptionEvent(...true...) is set
    virtual void SMSReception(SMSMessageRef newMessage,
                              SMSMessageType messageType);

    // CB reception
    // only called if setSMSReceptionEvent(...true...) is set
    // storage of CBM in ME is not supported by the standard
    virtual void CBReception(CBMessageRef newMessage);

    // SMS reception indication (called when SMS is not delivered to TE
    // but stored in ME memory)
    virtual void SMSReceptionIndication(string storeName, unsigned int index,
                                        SMSMessageType messageType);

    // RING indication
    virtual void ringIndication();

    friend class gsmlib::GsmAt;
  };
};

#endif // GSM_EVENT_H
