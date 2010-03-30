#include <gsmlib/gsm_unix_serial.h>
#include <gsmlib/gsm_sorted_sms_store.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <iostream>
#include <strstream>

using namespace std;
using namespace gsmlib;

int main(int argc, char *argv[])
{
  try
  {
    // open SMS store file
    SortedSMSStore sms((string)"sms.sms");
    
    // print all entries
    cout << "Entries in sms.sms<0>:" << endl;
    for (SortedSMSStore::iterator i = sms.begin(); i != sms.end(); ++i)
      cout << "Entry#" << i->index() << ":" << endl
           << i->message()->toString() << endl;

    // insert some entries
    cout << "Inserting some entries" << endl;
    SMSMessageRef smsMessage;
    // test two SMS message I have received
    smsMessage = SMSMessage::decode("079194710167120004038571F1390099406180904480A0D41631067296EF7390383D07CD622E58CD95CB81D6EF39BDEC66BFE7207A794E2FBB4320AFB82C07E56020A8FC7D9687DBED32285C9F83A06F769A9E5EB340D7B49C3E1FA3C3663A0B24E4CBE76516680A7FCBE920725A5E5ED341F0B21C346D4E41E1BA790E4286DDE4BC0BD42CA3E5207258EE1797E5A0BA9B5E9683C86539685997EBEF61341B249BC966");
    sms.insert(SMSStoreEntry(smsMessage));

    smsMessage = SMSMessage::decode("0791947101671200040B851008050001F23900892171410155409FCEF4184D07D9CBF273793E2FBB432062BA0CC2D2E5E16B398D7687C768FADC5E96B3DFF3BAFB0C62EFEB663AC8FD1EA341E2F41CA4AFB741329A2B2673819C75BABEEC064DD36590BA4CD7D34149B4BC0C3A96EF69B77B8C0EBBC76550DD4D0699C3F8B21B344D974149B4BCEC0651CB69B6DBD53AD6E9F331BA9C7683C26E102C8683BD6A30180C04ABD900");
    sms.insert(SMSStoreEntry(smsMessage));

    smsMessage = new SMSSubmitMessage("submit me", "0177123456");
    sms.insert(SMSStoreEntry(smsMessage));

    SMSSubmitMessage *subsms = new SMSSubmitMessage();
    subsms->setUserData("This is a submit message, isn't it?");
    smsMessage = subsms;
    sms.insert(SMSStoreEntry(smsMessage));

    // print all entries
    cout << "Entries in sms.sms<1>:" << endl;
    for (SortedSMSStore::iterator i = sms.begin(); i != sms.end(); ++i)
      cout << "Entry#" << i->index() << ":" << endl
           << i->message()->toString() << endl;

    // sort by telephone number
    sms.setSortOrder(ByAddress);

    // print all entries
    cout << "Entries in sms.sms<2>:" << endl;
    for (SortedSMSStore::iterator i = sms.begin(); i != sms.end(); ++i)
      cout << "Entry#" << i->index() << ":" << endl
           << i->message()->toString() << endl;

    // write back to file
    cout << "Writing back to file" << endl;
    sms.sync();

  }
  catch (GsmException &ge)
  {
    cerr << "GsmException '" << ge.what() << "'" << endl;
    return 1;
  }
  return 0;
}
