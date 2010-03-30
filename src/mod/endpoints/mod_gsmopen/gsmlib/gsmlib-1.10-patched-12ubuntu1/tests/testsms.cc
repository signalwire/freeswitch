// *************************************************************************
// * GSM TA/ME library
// *
// * File:    testsms.cc
// *
// * Purpose: Test coder and encoder for SMS TPDUs
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 17.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_sms.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

int main(int argc, char *argv[])
{
  string pdu;
  SMSMessageRef sms;
  // test two SMS message I have received
  sms = SMSMessage::decode("079194710167120004038571F1390099406180904480A0D41631067296EF7390383D07CD622E58CD95CB81D6EF39BDEC66BFE7207A794E2FBB4320AFB82C07E56020A8FC7D9687DBED32285C9F83A06F769A9E5EB340D7B49C3E1FA3C3663A0B24E4CBE76516680A7FCBE920725A5E5ED341F0B21C346D4E41E1BA790E4286DDE4BC0BD42CA3E5207258EE1797E5A0BA9B5E9683C86539685997EBEF61341B249BC966");
  cout << sms->toString() << endl;

  sms = SMSMessage::decode("0791947101671200040B851008050001F23900892171410155409FCEF4184D07D9CBF273793E2FBB432062BA0CC2D2E5E16B398D7687C768FADC5E96B3DFF3BAFB0C62EFEB663AC8FD1EA341E2F41CA4AFB741329A2B2673819C75BABEEC064DD36590BA4CD7D34149B4BC0C3A96EF69B77B8C0EBBC76550DD4D0699C3F8B21B344D974149B4BCEC0651CB69B6DBD53AD6E9F331BA9C7683C26E102C8683BD6A30180C04ABD900");
  cout << sms->toString() << endl;

  // test SMS decoding and encoding for messages with alphanumeric
  // destination address
  sms = SMSMessage::decode("07911497941902F00414D0E474989D769F5DE4320839001040122151820000");
  cout << sms->toString() << endl;
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu);
  cout << sms->toString() << endl;

  // test all message types
  sms = new SMSDeliverMessage();
  cout << sms->toString() << endl;
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu);
  cout << sms->toString() << endl;

  // test all message types
  sms = new SMSDeliverReportMessage();
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu, false);
  cout << sms->toString() << endl;

  // test all message types
  sms = new SMSStatusReportMessage();
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu);
  cout << sms->toString() << endl;

  // test all message types
  sms = new SMSCommandMessage();
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu, false);
  cout << sms->toString() << endl;

  // test all message types
  SMSSubmitMessage *subsms = new SMSSubmitMessage();
  subsms->setUserData("This is a submit message, isn't it?");
  sms = subsms;
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu, false);
  cout << sms->toString() << endl;
  
  // test all message types
  sms = new SMSSubmitReportMessage();
  pdu = sms->encode();
  sms = SMSMessage::decode(pdu);
  cout << sms->toString() << endl;
  return 0;
}
