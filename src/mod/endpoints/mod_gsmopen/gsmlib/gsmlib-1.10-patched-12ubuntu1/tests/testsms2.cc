#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#endif
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <strstream>
#include <iostream>

using namespace std;
using namespace gsmlib;

int main(int argc, char *argv[])
{
  try
  {
    cout << (string)"Opening device " + argv[1] << endl;
#ifdef WIN32
    Ref<Port> port = new Win32SerialPort((string)argv[1], 38400);
#else
	Ref<Port> port = new UnixSerialPort((string)argv[1], B38400);
#endif

    cout << "Creating MeTa object" << endl;
    MeTa m(port);

    cout << "Setting message service level to 1" << endl;
    m.setMessageService(1);

    vector<string> storeList = m.getSMSStoreNames();

    for (vector<string>::iterator stn = storeList.begin();
         stn != storeList.end(); ++stn)
    {
      cout << "Getting store \"" << *stn << "\"" << endl;
      SMSStoreRef st = m.getSMSStore(*stn);

      SMSMessageRef sms;
      cout << "Creating SMS Submit Message and putting it into store" << endl;
      SMSSubmitMessage *subsms = new SMSSubmitMessage();
//       Address scAddr("+491710760000");
//       subsms->setServiceCentreAddress(scAddr);
      Address destAddr("0177123456");
      subsms->setDestinationAddress(destAddr);
      subsms->setUserData("This message was sent from the store.");
      TimePeriod tp;
      tp._format = TimePeriod::Relative;
      tp._relativeTime = 100;
      /*subsms->setValidityPeriod(tp);
      subsms->setValidityPeriodFormat(tp._format);
      subsms->setStatusReportRequest(true);*/
      sms = subsms;
      SMSStore::iterator smsIter = st->insert(st->end(), SMSStoreEntry(sms));
      cout << "Message entered at index #"
           << smsIter - st->begin() << endl;

      //m.sendSMS(sms);
      SMSMessageRef ackPdu;
      int messageRef = smsIter->send(ackPdu);
      cout << "Message reference: " << messageRef << endl
           << "ACK PDU:" << endl
           << (ackPdu.isnull() ? "no ack pdu" : ackPdu->toString())
           << endl;

      /*      cout << "Erasing all unsent messages" << endl;
      for (SMSStore::iterator s = st->begin(); s != st->end(); ++s)
        if (! s->empty() && s->status() == SMSStoreEntry::StoredUnsent)
        st->erase(s);*/

      cout << "Printing store \"" << *stn << "\"" << endl;
      for (SMSStore::iterator s = st->begin(); s != st->end(); ++s)
        if (! s->empty())
          cout << s->message()->toString();

      break;                    // only do one store
    }
  }
  catch (GsmException &ge)
  {
    cerr << "GsmException '" << ge.what() << "'" << endl;
    return 1;
  }
  return 0;
}
