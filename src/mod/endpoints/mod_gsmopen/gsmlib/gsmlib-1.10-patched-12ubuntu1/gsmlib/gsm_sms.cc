// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sms.cc
// *
// * Purpose: SMS functions
// *          (ETSI GSM 07.05)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 16.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_sysdep.h>
#include <gsmlib/gsm_sms.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_parser.h>
#include <gsmlib/gsm_me_ta.h>
#include <strstream>
#include <string>

using namespace std;
using namespace gsmlib;

// local constants

static const string dashes =
"---------------------------------------------------------------------------";

// SMSMessage members

Ref<SMSMessage> SMSMessage::decode(string pdu,
                                   bool SCtoMEdirection,
                                   GsmAt *at) throw(GsmException)
{
  Ref<SMSMessage> result;
  SMSDecoder d(pdu);
  d.getAddress(true);
  MessageType messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  if (SCtoMEdirection)
    // TPDUs from SC to ME
    switch (messageTypeIndicator)
    {
    case SMS_DELIVER:
      result = new SMSDeliverMessage(pdu);
      break;

    case SMS_STATUS_REPORT:
      result = new SMSStatusReportMessage(pdu);
      break;

    case SMS_SUBMIT_REPORT:
      // observed with Motorola Timeport 260, the SCtoMEdirection can
      // be wrong in this case
      if (at != NULL && at->getMeTa().getCapabilities()._wrongSMSStatusCode)
        result = new SMSSubmitMessage(pdu);
      else
        result = new SMSSubmitReportMessage(pdu);
      break;

    default:
      throw GsmException(_("unhandled SMS TPDU type"), OtherError);
    }
  else
    // TPDUs from ME to SC
    switch (messageTypeIndicator)
    {
    case SMS_SUBMIT:
      result = new SMSSubmitMessage(pdu);
      break;

    case SMS_DELIVER_REPORT:
      result = new SMSDeliverReportMessage(pdu);
      break;

    case SMS_COMMAND:
      result = new SMSCommandMessage(pdu);
      break;

    default:
      throw GsmException(_("unhandled SMS TPDU type"), OtherError);
    }
  result->_at = at;
  return result;
}

Ref<SMSMessage> SMSMessage::decode(istream& s) throw(gsmlib::GsmException)
{
  string pdu;
  unsigned char ScToMe; 
	
  s >> ScToMe;
  s >> pdu;

  return decode(pdu,ScToMe=='S');
}

unsigned char SMSMessage::send(Ref<SMSMessage> &ackPdu)
  throw(GsmException)
{
  if (_messageTypeIndicator != SMS_SUBMIT &&
      _messageTypeIndicator != SMS_COMMAND)
    throw GsmException(_("can only send SMS-SUBMIT and SMS-COMMAND TPDUs"),
                       ParameterError);

  if (_at.isnull())
    throw GsmException(_("no device given for sending SMS"), ParameterError);

  string pdu = encode();
  Parser p(_at->sendPdu("+CMGS=" +
                        intToStr(pdu.length() / 2 - getSCAddressLen()),
                        "+CMGS:", pdu));
  unsigned char messageReference = p.parseInt();

  if (p.parseComma(true))
  {
    string pdu = p.parseEol();

    // add missing service centre address if required by ME
    if (! _at->getMeTa().getCapabilities()._hasSMSSCAprefix)
      pdu = "00" + pdu;

    ackPdu = SMSMessage::decode(pdu);
  }
  else
    ackPdu = SMSMessageRef();

  return messageReference;
}

unsigned char SMSMessage::send() throw(GsmException)
{
  SMSMessageRef mref;
  return send(mref);
}

unsigned int SMSMessage::getSCAddressLen()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  return e.getLength();
}

unsigned char SMSMessage::userDataLength() const
{
  unsigned int udhl = _userDataHeader.length();
  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
    return _userData.length() + (udhl ? ((1 + udhl) * 8 + 6) / 7 : 0);
  else
    return _userData.length() + (udhl ? (1 + udhl) : 0);
}

ostream& SMSMessage::operator<<(ostream& s)
{
  unsigned char ScToMe;
	
  if (dynamic_cast<SMSDeliverMessage*>(this) || 
      dynamic_cast<SMSStatusReportMessage*>(this) || 
      dynamic_cast<SMSSubmitReportMessage*>(this))
  {
    ScToMe = 'S';
  }
  else if (dynamic_cast<SMSSubmitMessage*>(this) || 
           dynamic_cast<SMSCommandMessage*>(this) || 
           dynamic_cast<SMSDeliverReportMessage*>(this))
  {
    ScToMe = 'M';
  }
  else
  {
    throw GsmException(_("unhandled SMS TPDU type"), OtherError);
  }

  s << ScToMe;
  return s << encode();
}

// SMSMessage::SMSMessage(SMSMessage &m)
// {
//   _at = m._at;
  
// }

// SMSMessage &SMSMessage::operator=(SMSMessage &m)
// {
// }

SMSMessage::~SMSMessage() {}

// SMSDeliverMessage members

void SMSDeliverMessage::init()
{
  _messageTypeIndicator = SMS_DELIVER;
  _moreMessagesToSend = false;
  _replyPath = false;
  _statusReportIndication = false;
  _protocolIdentifier = 0;
}

SMSDeliverMessage::SMSDeliverMessage()
{
  init();
}

SMSDeliverMessage::SMSDeliverMessage(string pdu) throw(GsmException)
{
  SMSDecoder d(pdu);
  _serviceCentreAddress = d.getAddress(true);
  _messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  assert(_messageTypeIndicator == SMS_DELIVER);
  _moreMessagesToSend = d.getBit(); // bit 2
  d.getBit();                   // bit 3
  d.getBit();                   // bit 4
  _statusReportIndication = d.getBit(); // bit 5
  bool userDataHeaderIndicator = d.getBit(); // bit 6
  _replyPath = d.getBit();      // bit 7
  _originatingAddress = d.getAddress();
  _protocolIdentifier = d.getOctet();
  _dataCodingScheme = d.getOctet();
  _serviceCentreTimestamp = d.getTimestamp();
  unsigned char userDataLength = d.getOctet();
  d.markSeptet();

  if (userDataHeaderIndicator)
  {
    _userDataHeader.decode(d);
    if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
      userDataLength -= ((_userDataHeader.length() + 1) * 8 + 6) / 7;
    else
      userDataLength -= ((string)_userDataHeader).length() + 1;
  }
  else
    _userDataHeader = UserDataHeader();

  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
  {                             // userDataLength is length in septets
    _userData = d.getString(userDataLength);
    _userData = gsmToLatin1(_userData);
  }
  else
  {                             // userDataLength is length in octets
    unsigned char *s = 
      (unsigned char*)alloca(sizeof(unsigned char) * userDataLength);
    d.getOctets(s, userDataLength);
    _userData.assign((char*)s, (unsigned int)userDataLength);
  }
}

string SMSDeliverMessage::encode()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  e.set2Bits(_messageTypeIndicator); // bits 0..1
  e.setBit(_moreMessagesToSend); // bit 2
  e.setBit();                   // bit 3
  e.setBit();                   // bit 4
  e.setBit(_statusReportIndication); // bit 5
  e.setBit(_userDataHeader.length() != 0); // bit 6
  e.setBit(_replyPath);         // bit 7
  e.setAddress(_originatingAddress);
  e.setOctet(_protocolIdentifier);
  e.setOctet(_dataCodingScheme);
  e.setTimestamp(_serviceCentreTimestamp);
  e.setOctet(userDataLength());
  e.markSeptet();
  if (_userDataHeader.length()) _userDataHeader.encode(e);
  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
    e.setString(latin1ToGsm(_userData));
  else
    e.setOctets((unsigned char*)_userData.data(), _userData.length());
  return e.getHexString();
}

string SMSDeliverMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: SMS-DELIVER") << endl
     << _("SC address: '") << _serviceCentreAddress._number << "'" << endl
     << _("More messages to send: ") << _moreMessagesToSend << endl
     << _("Reply path: ") << _replyPath << endl
     << _("User data header indicator: ")
     << (_userDataHeader.length()!=0) << endl
     << _("Status report indication: ") << _statusReportIndication << endl
     << _("Originating address: '") << _originatingAddress._number 
     << "'" << endl
     << _("Protocol identifier: 0x") << hex
     << (unsigned int)_protocolIdentifier << dec << endl
     << _("Data coding scheme: ") << _dataCodingScheme.toString() << endl
     << _("SC timestamp: ") << _serviceCentreTimestamp.toString() << endl
     << _("User data length: ") << (int)userDataLength() << endl
     << _("User data header: 0x")
     << bufToHex((unsigned char*)
                 ((string)_userDataHeader).data(),
                 ((string)_userDataHeader).length())
     << endl
     << _("User data: '") << _userData << "'" << endl
     << dashes << endl << endl
     << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result;
}

Address SMSDeliverMessage::address() const
{
  return _originatingAddress;
}

Ref<SMSMessage> SMSDeliverMessage::clone()
{
  Ref<SMSMessage> result = new SMSDeliverMessage(*this);
  return result;
}

// SMSSubmitMessage members

void SMSSubmitMessage::init()
{
  // set everything to sensible default values
  _messageTypeIndicator = SMS_SUBMIT;
  _validityPeriodFormat = TimePeriod::Relative; 
  _validityPeriod._format = TimePeriod::Relative;
  _validityPeriod._relativeTime = 168; // 2 days
  _statusReportRequest = false;
  _replyPath = false;
  _rejectDuplicates = true;
  _messageReference = 0;
  _protocolIdentifier = 0;
}

SMSSubmitMessage::SMSSubmitMessage()
{
  init();
}

SMSSubmitMessage::SMSSubmitMessage(string pdu) throw(GsmException)
{ 
  SMSDecoder d(pdu);
  _serviceCentreAddress = d.getAddress(true);
  _messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  assert(_messageTypeIndicator == SMS_SUBMIT);
  _rejectDuplicates = d.getBit(); // bit 2
  _validityPeriodFormat = (TimePeriod::Format)d.get2Bits(); // bits 3..4
  _statusReportRequest = d.getBit(); // bit 5
  bool userDataHeaderIndicator = d.getBit(); // bit 6
  _replyPath = d.getBit();      // bit 7
  _messageReference = d.getOctet();
  _destinationAddress = d.getAddress();
  _protocolIdentifier = d.getOctet();
  _dataCodingScheme = d.getOctet();
  if (_validityPeriodFormat != TimePeriod::NotPresent)
    _validityPeriod = d.getTimePeriod(_validityPeriodFormat);
  unsigned char userDataLength = d.getOctet();
  d.markSeptet();

  if (userDataHeaderIndicator)
  {
    _userDataHeader.decode(d);
    if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
      userDataLength -= ((_userDataHeader.length() + 1) * 8 + 6) / 7;
    else
      userDataLength -= ((string)_userDataHeader).length() + 1;
  }
  else
    _userDataHeader = UserDataHeader();

  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
  {                             // userDataLength is length in septets
    _userData = d.getString(userDataLength);
    _userData = gsmToLatin1(_userData);
  }
  else
  {                             // _userDataLength is length in octets
    unsigned char *s =
      (unsigned char*)alloca(sizeof(unsigned char) * userDataLength);
    d.getOctets(s, userDataLength);
    _userData.assign((char*)s, userDataLength);
  }
}

SMSSubmitMessage::SMSSubmitMessage(string text, string number)
{
  init();
  _destinationAddress = Address(number);
  _userData = text;
}

string SMSSubmitMessage::encode()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  e.set2Bits(_messageTypeIndicator); // bits 0..1
  e.setBit(_rejectDuplicates); // bit 2
  e.set2Bits(_validityPeriodFormat); // bits 3..4
  e.setBit(_statusReportRequest); // bit 5
  bool userDataHeaderIndicator = _userDataHeader.length() != 0;
  e.setBit(userDataHeaderIndicator); // bit 6
  e.setBit(_replyPath);       // bit 7
  e.setOctet(_messageReference);
  e.setAddress(_destinationAddress);
  e.setOctet(_protocolIdentifier);
  e.setOctet(_dataCodingScheme);
  e.setTimePeriod(_validityPeriod);
  e.setOctet(userDataLength());
  e.markSeptet();
  if (userDataHeaderIndicator) _userDataHeader.encode(e);
  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
    e.setString(latin1ToGsm(_userData));
  else
    e.setOctets((unsigned char*)_userData.data(), _userData.length());
  return e.getHexString();
}

string SMSSubmitMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: SMS-SUBMIT") << endl
     << _("SC address: '") << _serviceCentreAddress._number << "'" << endl
     << _("Reject duplicates: ") << _rejectDuplicates << endl
     << _("Validity period format: ");
  switch (_validityPeriodFormat)
  {
  case TimePeriod::NotPresent:
    os << _("not present");
    break;
  case TimePeriod::Relative:
    os << _("relative");
    break;
  case TimePeriod::Absolute:
    os << _("absolute");
    break;
  default:
    os << _("unknown");
    break;
  }
  os << endl
     << _("Reply path: ") << _replyPath << endl
     << _("User data header indicator: ")
     << (_userDataHeader.length()!=0) << endl
     << _("Status report request: ") << _statusReportRequest << endl
     << _("Message reference: ") << (unsigned int)_messageReference << endl
     << _("Destination address: '") << _destinationAddress._number 
     << "'" << endl
     << _("Protocol identifier: 0x") << hex 
     << (unsigned int)_protocolIdentifier << dec << endl
     << _("Data coding scheme: ") << _dataCodingScheme.toString() << endl
     << _("Validity period: ") << _validityPeriod.toString() << endl
     << _("User data length: ") << (int)userDataLength() << endl
     << _("User data header: 0x") << bufToHex((unsigned char*)
                                              ((string)_userDataHeader).data(),
                                              _userDataHeader.length())
     << endl
     << _("User data: '") << _userData << "'" << endl
     << dashes << endl << endl
     << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result; 
}

Address SMSSubmitMessage::address() const
{
  return _destinationAddress;
}

Ref<SMSMessage> SMSSubmitMessage::clone()
{
  Ref<SMSMessage> result = new SMSSubmitMessage(*this);
  return result;
}

// SMSStatusReportMessage members

void SMSStatusReportMessage::init()
{
  _messageTypeIndicator = SMS_STATUS_REPORT;
  _moreMessagesToSend = false;
  _statusReportQualifier = false;
  _messageReference = 0;
  _status = SMS_STATUS_RECEIVED;
}

SMSStatusReportMessage::SMSStatusReportMessage(string pdu) throw(GsmException)
{
  SMSDecoder d(pdu);
  _serviceCentreAddress = d.getAddress(true);
  _messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  assert(_messageTypeIndicator == SMS_STATUS_REPORT);
  _moreMessagesToSend = d.getBit(); // bit 2
  d.getBit();                   // bit 3
  d.getBit();                   // bit 4
  _statusReportQualifier = d.getBit(); // bit 5
  _messageReference = d.getOctet();
  _recipientAddress = d.getAddress();
  _serviceCentreTimestamp = d.getTimestamp();
  _dischargeTime = d.getTimestamp();
  _status = d.getOctet();
}

string SMSStatusReportMessage::encode()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  e.set2Bits(_messageTypeIndicator); // bits 0..1
  e.setBit(_moreMessagesToSend); // bit 2
  e.setBit();                   // bit 3
  e.setBit();                   // bit 4
  e.setBit(_statusReportQualifier); // bit 5
  e.setOctet(_messageReference);
  e.setAddress(_recipientAddress);
  e.setTimestamp(_serviceCentreTimestamp);
  e.setTimestamp(_dischargeTime);
  e.setOctet(_status);
  return e.getHexString();
}

string SMSStatusReportMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: SMS-STATUS-REPORT") << endl
     << _("SC address: '") << _serviceCentreAddress._number << "'" << endl
     << _("More messages to send: ") << _moreMessagesToSend << endl
     << _("Status report qualifier: ") << _statusReportQualifier << endl
     << _("Message reference: ") << (unsigned int)_messageReference << endl
     << _("Recipient address: '") << _recipientAddress._number << "'" << endl
     << _("SC timestamp: ") << _serviceCentreTimestamp.toString() << endl
     << _("Discharge time: ") << _dischargeTime.toString() << endl
     << _("Status: 0x") << hex << (unsigned int)_status << dec
     << " '" << getSMSStatusString(_status) << "'" << endl
     << dashes << endl << endl
     << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result; 
}

Address SMSStatusReportMessage::address() const
{
  return _recipientAddress;
}

Ref<SMSMessage> SMSStatusReportMessage::clone()
{
  Ref<SMSMessage> result = new SMSStatusReportMessage(*this);
  return result;
}

// SMSCommandMessage members

void SMSCommandMessage::init()
{
  _messageTypeIndicator = SMS_COMMAND;
  _messageReference = 0;
  _statusReportRequest = true;
  _protocolIdentifier = 0;
  _commandType = EnquireSM;
  _messageNumber = 0;
  _commandDataLength = 0; 
}

SMSCommandMessage::SMSCommandMessage(string pdu) throw(GsmException)
{
  SMSDecoder d(pdu);
  _serviceCentreAddress = d.getAddress(true);
  _messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  assert(_messageTypeIndicator == SMS_COMMAND);
  d.getBit();                   // bit 2
  d.getBit();                   // bit 3
  d.getBit();                   // bit 4
  _statusReportRequest = d.getBit(); // bit 5
  _messageReference = d.getOctet();
  _protocolIdentifier = d.getOctet();
  _commandType = d.getOctet();
  _messageNumber = d.getOctet();
  _destinationAddress = d.getAddress();
  _commandDataLength = d.getOctet();
  unsigned char *s = 
      (unsigned char*)alloca(sizeof(unsigned char) * _commandDataLength);
  d.getOctets(s, _commandDataLength);
}

string SMSCommandMessage::encode()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  e.set2Bits(_messageTypeIndicator); // bits 0..1
  e.setBit();                   // bit 2
  e.setBit();                   // bit 3
  e.setBit();                   // bit 4
  e.setBit(_statusReportRequest); // bit 5
  e.setOctet(_messageReference);
  e.setOctet(_protocolIdentifier);
  e.setOctet(_commandType);
  e.setOctet(_messageNumber);
  e.setAddress(_destinationAddress);
  e.setOctet(_commandData.length());
  e.setOctets((const unsigned char*)_commandData.data(),
              (short unsigned int)_commandData.length());
  return e.getHexString();
}

string SMSCommandMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: SMS-COMMAND") << endl
     << _("SC address: '") << _serviceCentreAddress._number << "'" << endl
     << _("Message reference: ") << (unsigned int)_messageReference << endl
     << _("Status report request: ") << _statusReportRequest << endl
     << _("Protocol identifier: 0x") << hex 
     << (unsigned int)_protocolIdentifier << dec << endl
     << _("Command type: 0x") << hex << (unsigned int)_commandType
     << dec << endl
     << _("Message number: ") << (unsigned int)_messageNumber << endl
     << _("Destination address: '") << _destinationAddress._number 
     << "'" << endl
     << _("Command data length: ") << (unsigned int)_commandDataLength << endl
     << _("Command data: '") << _commandData << "'" << endl
     << dashes << endl << endl
     << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result; 
}

Address SMSCommandMessage::address() const
{
  return _destinationAddress;
}

Ref<SMSMessage> SMSCommandMessage::clone()
{
  Ref<SMSMessage> result = new SMSCommandMessage(*this);
  return result;
}

// SMSDeliverReportMessage members

void SMSDeliverReportMessage::init()
{
  _messageTypeIndicator = SMS_DELIVER_REPORT;
  _protocolIdentifierPresent = false;
  _dataCodingSchemePresent = false;
  _userDataLengthPresent = false;
}

SMSDeliverReportMessage::SMSDeliverReportMessage(string pdu)
  throw(GsmException)
{
  SMSDecoder d(pdu);
  _serviceCentreAddress = d.getAddress(true);
  _messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  assert(_messageTypeIndicator == SMS_DELIVER_REPORT);
  d.alignOctet();               // skip to parameter indicator
  _protocolIdentifierPresent = d.getBit(); // bit 0
  _dataCodingSchemePresent = d.getBit(); // bit 1
  _userDataLengthPresent = d.getBit(); // bit 2
  if (_protocolIdentifierPresent)
    _protocolIdentifier = d.getOctet();
  if (_dataCodingSchemePresent)
    _dataCodingScheme = d.getOctet();
  if (_userDataLengthPresent)
  {
    unsigned char userDataLength = d.getOctet();
    d.markSeptet();
    if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
    {
      _userData = d.getString(userDataLength);
      _userData = gsmToLatin1(_userData);
    }
    else
    {                           // userDataLength is length in octets
      unsigned char *s =
        (unsigned char*)alloca(sizeof(unsigned char) * userDataLength);
      d.getOctets(s, userDataLength);
      _userData.assign((char*)s, userDataLength);
    }
  }
}

string SMSDeliverReportMessage::encode()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  e.set2Bits(_messageTypeIndicator); // bits 0..1
  e.alignOctet();               // skip to parameter indicator
  e.setBit(_protocolIdentifierPresent); // bit 0
  e.setBit(_dataCodingSchemePresent); // bit 1
  e.setBit(_userDataLengthPresent); // bit 2
  if (_protocolIdentifierPresent)
    e.setOctet(_protocolIdentifier);
  if (_dataCodingSchemePresent)
    e.setOctet(_dataCodingScheme);
  if (_userDataLengthPresent)
  {
    unsigned char userDataLength = _userData.length();
    e.setOctet(userDataLength);
    if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
      e.setString(latin1ToGsm(_userData));
    else
      e.setOctets((unsigned char*)_userData.data(), userDataLength);
  }
  return e.getHexString();
}

string SMSDeliverReportMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: SMS-DELIVER-REPORT") << endl
     << _("SC address: '") << _serviceCentreAddress._number << "'" << endl
     << _("Protocol identifier present: ") << _protocolIdentifierPresent
     << endl
     << _("Data coding scheme present: ") << _dataCodingSchemePresent << endl
     << _("User data length present: ") << _userDataLengthPresent << endl;
  if (_protocolIdentifierPresent)
    os << _("Protocol identifier: 0x") << hex
       << (unsigned int)_protocolIdentifier
       << dec << endl;
  if (_dataCodingSchemePresent)
    os << _("Data coding scheme: ") << _dataCodingScheme.toString() << endl;
  if (_userDataLengthPresent)
    os << _("User data length: ") << (int)userDataLength() << endl
       << _("User data: '") << _userData << "'" << endl;
  os << dashes << endl << endl
     << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result; 
}

Address SMSDeliverReportMessage::address() const
{
  assert(0);                    // not address, should not be in SMS store
  return Address();
}

Ref<SMSMessage> SMSDeliverReportMessage::clone()
{
  Ref<SMSMessage> result = new SMSDeliverReportMessage(*this);
  return result;
}

// SMSSubmitReportMessage members

void SMSSubmitReportMessage::init()
{
  _messageTypeIndicator = SMS_SUBMIT_REPORT;
  _protocolIdentifierPresent = false;
  _dataCodingSchemePresent = false;
  _userDataLengthPresent = false;
}

SMSSubmitReportMessage::SMSSubmitReportMessage(string pdu) throw(GsmException)
{
  SMSDecoder d(pdu);
  _serviceCentreAddress = d.getAddress(true);
  _messageTypeIndicator = (MessageType)d.get2Bits(); // bits 0..1
  assert(_messageTypeIndicator == SMS_SUBMIT_REPORT);
  _serviceCentreTimestamp = d.getTimestamp();
  _protocolIdentifierPresent = d.getBit(); // bit 0
  _dataCodingSchemePresent = d.getBit(); // bit 1
  _userDataLengthPresent = d.getBit(); // bit 2
  if (_protocolIdentifierPresent)
    _protocolIdentifier = d.getOctet();
  if (_dataCodingSchemePresent)
    _dataCodingScheme = d.getOctet();
  if (_userDataLengthPresent)
  {
    unsigned char userDataLength = d.getOctet();
    d.markSeptet();
    if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
    {
      _userData = d.getString(userDataLength);
      _userData = gsmToLatin1(_userData);
    }
    else
    {                           // _userDataLength is length in octets
      unsigned char *s =
        (unsigned char*)alloca(sizeof(unsigned char) * userDataLength);
      d.getOctets(s, userDataLength);
      _userData.assign((char*)s, userDataLength);
    }
  }
}

string SMSSubmitReportMessage::encode()
{
  SMSEncoder e;
  e.setAddress(_serviceCentreAddress, true);
  e.set2Bits(_messageTypeIndicator); // bits 0..1
  e.setTimestamp(_serviceCentreTimestamp);
  e.setBit(_protocolIdentifierPresent); // bit 0
  e.setBit(_dataCodingSchemePresent); // bit 1
  e.setBit(_userDataLengthPresent); // bit 2
  if (_protocolIdentifierPresent)
    e.setOctet(_protocolIdentifier);
  if (_dataCodingSchemePresent)
    e.setOctet(_dataCodingScheme);
  if (_userDataLengthPresent)
  {
    e.setOctet(userDataLength());
    if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
      e.setString(latin1ToGsm(_userData));
    else
      e.setOctets((unsigned char*)_userData.data(), _userData.length());
  }
  return e.getHexString();
}

string SMSSubmitReportMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: SMS-SUBMIT-REPORT") << endl
     << _("SC address: '") << _serviceCentreAddress._number << "'" << endl
     << _("SC timestamp: ") << _serviceCentreTimestamp.toString() << endl
     << _("Protocol identifier present: ") << _protocolIdentifierPresent
     << endl
     << _("Data coding scheme present: ") << _dataCodingSchemePresent << endl
     << _("User data length present: ") << _userDataLengthPresent << endl;
  if (_protocolIdentifierPresent)
    os << _("Protocol identifier: 0x") << hex
       << (unsigned int)_protocolIdentifier
       << dec << endl;
  if (_dataCodingSchemePresent)
    os << _("Data coding scheme: ") << _dataCodingScheme.toString() << endl;
  if (_userDataLengthPresent)
    os << _("User data length: ") << (int)userDataLength() << endl
       << _("User data: '") << _userData << "'" << endl;
  os << dashes << endl << endl
     << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result; 
}

Address SMSSubmitReportMessage::address() const
{
  assert(0);                    // not address, should not be in SMS store
  return Address();
}

Ref<SMSMessage> SMSSubmitReportMessage::clone()
{
  Ref<SMSMessage> result = new SMSSubmitReportMessage(*this);
  return result;
}

