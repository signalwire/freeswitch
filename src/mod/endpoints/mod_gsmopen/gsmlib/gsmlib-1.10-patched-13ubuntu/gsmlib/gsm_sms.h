// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sms.h
// *
// * Purpose: SMS functions
// *          (ETSI GSM 07.05)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 16.5.1999
// *************************************************************************

#ifndef GSM_SMS_H
#define GSM_SMS_H

#include <gsmlib/gsm_sms_codec.h>
#include <gsmlib/gsm_error.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_at.h>
#include <string>
#include <vector>

using namespace std;

namespace gsmlib
{
  // forward declarations
  class SMSStore;
  class SMSMessage;

  // this class represents a single SMS message
  class SMSMessage : public RefBase
  {
  private:
    Ref<GsmAt> _at;             // connection to the device

  public:
    // possible values for message type indicator
    enum MessageType {SMS_DELIVER = 0, SMS_DELIVER_REPORT = 0,
                      SMS_STATUS_REPORT = 2, SMS_COMMAND = 2,
                      SMS_SUBMIT = 1, SMS_SUBMIT_REPORT = 1};

  protected:
    // fields of the different TPDUs
    // all PDUs
    string _userData;
    UserDataHeader _userDataHeader;
    Address _serviceCentreAddress;
    MessageType _messageTypeIndicator;// 2 bits
    DataCodingScheme _dataCodingScheme;

  public:
    // decode hexadecimal pdu string
    // return SMSMessage of the appropriate type
    // differentiate between SMS transfer directions SC to ME, ME to SC
    // also give GsmAt object for send()
    static Ref<SMSMessage> decode(string pdu,
                                  bool SCtoMEdirection = true,
                                  GsmAt *at = NULL)
      throw(GsmException);

    static Ref<SMSMessage> decode(istream& s) throw(GsmException);

    // encode pdu, return hexadecimal pdu string
    virtual string encode() = 0;

    // send this PDU
    // returns message reference and ACK-PDU (if requested)
    // only applicate to SMS-SUBMIT and SMS-COMMAND
    unsigned char send(Ref<SMSMessage> &ackPdu) throw(GsmException);
    
    // same as above, but ACK-PDU is discarded
    unsigned char send() throw(GsmException);

    // create textual representation of SMS
    virtual string toString() const = 0;

    // return deep copy of this message
    virtual Ref<SMSMessage> clone() = 0;

    // return length of SC address when encoded
    unsigned int getSCAddressLen();

    // accessor functions
    MessageType messageType() const {return _messageTypeIndicator;}
    Address serviceCentreAddress() const {return _serviceCentreAddress;}

    // provided for sorting messages by timestamp
    virtual Timestamp serviceCentreTimestamp() const {return Timestamp();}
    
    // return recipient, destination etc. address (for sorting by address)
    virtual Address address() const = 0;

    virtual void setUserData(string x) {_userData = x;}
    virtual string userData() const {return _userData;}
    
    // return the size of user data (including user data header)
    unsigned char userDataLength() const;

    // accessor functions
    virtual void setUserDataHeader(UserDataHeader x) {_userDataHeader = x;}
    virtual UserDataHeader userDataHeader() const {return _userDataHeader;}
    
    virtual DataCodingScheme dataCodingScheme() const
      {return _dataCodingScheme;}
    virtual void setDataCodingScheme(DataCodingScheme x)
      {_dataCodingScheme = x;}

    void setServiceCentreAddress(Address &x) {_serviceCentreAddress = x;}
    void setAt(Ref<GsmAt> at) {_at = at;}

    virtual ~SMSMessage();

    // print ASCII hex representation of message
    ostream& operator<<(ostream& s);

    // copy constructor and assignment
//     SMSMessage(SMSMessage &m);
//     SMSMessage &operator=(SMSMessage &m);

    friend class SMSStore;
  };

  // SMS-DELIVER TPDU
  class SMSDeliverMessage : public SMSMessage
  {
  private:
    // SMS-DELIVER PDU members (see GSM 03.40 section 9.2.2.1)
    bool _moreMessagesToSend;
    bool _replyPath;
    bool _statusReportIndication;
    Address _originatingAddress;
    unsigned char _protocolIdentifier; // octet
    Timestamp _serviceCentreTimestamp;

    // initialize members to sensible values
    void init();
    
  public:
    // constructor, sets sensible default values
    SMSDeliverMessage();

    // constructor with given pdu
    SMSDeliverMessage(string pdu) throw(GsmException);

    // encode pdu, return hexadecimal pdu string
    virtual string encode();

    // create textual representation of SMS
    virtual string toString() const;

    // inherited from SMSMessage
    Address address() const;
    Ref<SMSMessage> clone();

    // accessor functions
    bool moreMessagesToSend() const {return _moreMessagesToSend;}
    bool replyPath() const {return _replyPath;}
    bool statusReportIndication() const {return _statusReportIndication;}
    Address originatingAddress() const {return _originatingAddress;}
    unsigned char protocolIdentifier() const {return _protocolIdentifier;}
    Timestamp serviceCentreTimestamp() const {return _serviceCentreTimestamp;}

    void setMoreMessagesToSend(bool x) {_moreMessagesToSend = x;}
    void setReplyPath(bool x) {_replyPath = x;}
    void setStatusReportIndication(bool x) {_statusReportIndication = x;}
    void setOriginatingAddress(Address &x) {_originatingAddress = x;}
    void setProtocolIdentifier(unsigned char x) {_protocolIdentifier = x;}
    void setServiceCentreTimestamp(Timestamp &x) {_serviceCentreTimestamp = x;}

    virtual ~SMSDeliverMessage() {}
  };

  // SMS-SUBMIT TPDU
  class SMSSubmitMessage : public SMSMessage
  {
  private:
    // SMS-SUBMIT PDU (see GSM 03.40 section 9.2.2.2)
    bool _rejectDuplicates;
    TimePeriod::Format _validityPeriodFormat; // 2 bits
    bool _replyPath;
    bool _statusReportRequest;
    unsigned char _messageReference; // integer
    Address _destinationAddress;
    unsigned char _protocolIdentifier;
    TimePeriod _validityPeriod;

    // initialize members to sensible values
    void init();
    
  public:
    // constructor, sets sensible default values
    SMSSubmitMessage();

    // constructor with given pdu
    SMSSubmitMessage(string pdu) throw(GsmException);

    // convenience constructor
    // given the text and recipient telephone number
    SMSSubmitMessage(string text, string number);

    // encode pdu, return hexadecimal pdu string
    virtual string encode();

    // create textual representation of SMS
    virtual string toString() const;

    // inherited from SMSMessage
    Address address() const;
    Ref<SMSMessage> clone();

    // accessor functions
    bool rejectDuplicates() const {return _rejectDuplicates;}
    TimePeriod::Format validityPeriodFormat() const
      {return _validityPeriodFormat;}
    bool replyPath() const {return _replyPath;}
    bool statusReportRequest() const {return _statusReportRequest;}
    unsigned char messageReference() const {return _messageReference;}
    Address destinationAddress() const {return _destinationAddress;}
    unsigned char protocolIdentifier() const {return _protocolIdentifier;}
    TimePeriod validityPeriod() const {return _validityPeriod;}

    void setRejectDuplicates(bool x) {_rejectDuplicates = x;}
    void setValidityPeriodFormat(TimePeriod::Format &x)
      {_validityPeriodFormat = x;}
    void setReplyPath(bool x) {_replyPath = x;}
    void setStatusReportRequest(bool x) {_statusReportRequest = x;}
    void setMessageReference(unsigned char x) {_messageReference = x;}
    void setDestinationAddress(Address &x) {_destinationAddress = x;}
    void setProtocolIdentifier(unsigned char x) {_protocolIdentifier = x;}
    void setValidityPeriod(TimePeriod &x) {_validityPeriod = x;}
    
    virtual ~SMSSubmitMessage() {}
  };

  // SMS-STATUS-REPORT TPDU
  class SMSStatusReportMessage : public SMSMessage
  {
  private:
    // SMS-STATUS-REPORT PDU (see GSM 03.40 section 9.2.2.3)
    bool _moreMessagesToSend;
    bool _statusReportQualifier;
    unsigned char _messageReference;
    Address _recipientAddress;
    Timestamp _serviceCentreTimestamp;
    Timestamp _dischargeTime;
    unsigned char _status;      // octet
    
    // initialize members to sensible values
    void init();
    
  public:
    // constructor, sets sensible default values
    SMSStatusReportMessage() {init();}

    // constructor with given pdu
    SMSStatusReportMessage(string pdu) throw(GsmException);

    // encode pdu, return hexadecimal pdu string
    virtual string encode();

    // create textual representation of SMS
    virtual string toString() const;

    // inherited from SMSMessage
    Address address() const;
    Ref<SMSMessage> clone();

    // accessor functions
    bool moreMessagesToSend() const {return _moreMessagesToSend;}
    bool statusReportQualifier() const {return _statusReportQualifier;}
    unsigned char messageReference() const {return _messageReference;}
    Address recipientAddress() const {return _recipientAddress;}
    Timestamp serviceCentreTimestamp() const {return _serviceCentreTimestamp;}
    Timestamp dischargeTime() const {return _dischargeTime;}
    unsigned char status() const {return _status;}
    
    void setMoreMessagesToSend(bool x) {_moreMessagesToSend = x;}
    void setStatusReportQualifier(bool x) {_statusReportQualifier = x;}
    void setMessageReference(unsigned char x) {_messageReference = x;}
    void setRecipientAddress(Address x) {_recipientAddress = x;}
    void setServiceCentreTimestamp(Timestamp x) {_serviceCentreTimestamp = x;}
    void setDischargeTime(Timestamp x) {_serviceCentreTimestamp = x;}
    void setStatus(unsigned char x) {_status = x;}

    virtual ~SMSStatusReportMessage() {}
  };

  // SMS-COMMAND TPDU
  class SMSCommandMessage : public SMSMessage
  {
  public:
    // command types, other values are reserved or SC-specific
    enum CommandType {EnquireSM = 0, CancelStatusReportRequest = 1,
                      DeleteSubmittedSM = 2, EnalbeStatusReportRequest = 3};

  private:
    // SMS-COMMAND PDU (see GSM 03.40 section 9.2.2.4)
    unsigned char _messageReference;
    bool _statusReportRequest;
    unsigned char _protocolIdentifier;
    unsigned char _commandType;
    unsigned char _messageNumber;
    Address _destinationAddress;
    unsigned char _commandDataLength;
    string _commandData;

    // initialize members to sensible values
    void init();
    
  public:
    // constructor, sets sensible default values
    SMSCommandMessage() {init();}

    // constructor with given pdu
    SMSCommandMessage(string pdu) throw(GsmException);

    // encode pdu, return hexadecimal pdu string
    virtual string encode();

    // create textual representation of SMS
    virtual string toString() const;

    // inherited from SMSMessage
    Address address() const;
    Ref<SMSMessage> clone();

    // accessor functions
    unsigned char messageReference() const {return _messageReference;}
    bool statusReportRequest() const {return _statusReportRequest;}
    unsigned char protocolIdentifier() const {return _protocolIdentifier;}
    unsigned char commandType() const {return _commandType;}
    unsigned char messageNumber() const {return _messageNumber;}
    Address destinationAddress() const {return _destinationAddress;}
    unsigned char commandDataLength() const {return _commandDataLength;}
    string commandData() const {return _commandData;}

    void setMessageReference(unsigned char x) {_messageReference = x;}
    void setStatusReportRequest(bool x) {_statusReportRequest = x;}
    void setProtocolIdentifier(unsigned char x) {_protocolIdentifier = x;}
    void setCommandType(unsigned char x) {_commandType = x;}
    void setMessageNumber(unsigned char x) {_messageNumber = x;}
    void setDestinationAddress(Address &x) {_destinationAddress = x;}
    void setCommandDataLength(unsigned char x) {_commandDataLength = x;}
    void setCommandData(string x) {_commandData = x;}

    virtual ~SMSCommandMessage() {}
  };

  // SMS-DELIVER-REPORT TPDU for RP-ACK
  class SMSDeliverReportMessage : public SMSMessage
  {
  private:
    // SMS-DELIVER-REPORT PDU (see GSM 03.40 section 9.2.2.1a (II))
    bool _protocolIdentifierPresent; // parameter indicator
    bool _dataCodingSchemePresent;
    bool _userDataLengthPresent;
    unsigned char _protocolIdentifier;
    
    // initialize members to sensible values
    void init();
    
  public:
    // constructor, sets sensible default values
    SMSDeliverReportMessage() {init();}

    // constructor with given pdu
    SMSDeliverReportMessage(string pdu) throw(GsmException);

    // encode pdu, return hexadecimal pdu string
    virtual string encode();

    // create textual representation of SMS
    virtual string toString() const;

    // inherited from SMSMessage
    Address address() const;
    Ref<SMSMessage> clone();

    // accessor functions
    bool protocolIdentifierPresent() const {return _protocolIdentifierPresent;}
    bool dataCodingSchemePresent() const {return _dataCodingSchemePresent;}
    bool userDataLengthPresent() const {return _userDataLengthPresent;}
    unsigned char protocolIdentifier() const
      {assert(_protocolIdentifierPresent); return _protocolIdentifier;}
    DataCodingScheme dataCodingScheme() const
      {assert(_dataCodingSchemePresent); return _dataCodingScheme;}
    UserDataHeader userDataHeader() const
      {assert(_userDataLengthPresent); return _userDataHeader;}
    string userData() const
      {assert(_userDataLengthPresent); return _userData;}
    
    void setProtocolIdentifier(unsigned char x)
      {_protocolIdentifierPresent = true; _protocolIdentifier = x;}
    void setDataCodingScheme(DataCodingScheme x)
      {_dataCodingSchemePresent = true; _dataCodingScheme = x;}
    void setUserDataHeader(UserDataHeader x)
    {
      _userDataLengthPresent = true;
      _userDataHeader = x;
    }
    void setUserData(string x)
    {
      _userDataLengthPresent = true;
      _userData = x;
    }
    
    virtual ~SMSDeliverReportMessage() {}
  };

  // SMS-SUBMIT-REPORT TPDU for RP-ACK
  class SMSSubmitReportMessage : public SMSMessage
  {
  private:
    // SMS-SUBMIT-REPORT PDU (see GSM 03.40 section 9.2.2.2a (II))
    Timestamp _serviceCentreTimestamp;
    bool _protocolIdentifierPresent; // parameter indicator
    bool _dataCodingSchemePresent;
    bool _userDataLengthPresent;
    unsigned char _protocolIdentifier;
    DataCodingScheme _dataCodingScheme;

    // initialize members to sensible values
    void init();
    
  public:
    // constructor, sets sensible default values
    SMSSubmitReportMessage() {init();}

    // constructor with given pdu
    SMSSubmitReportMessage(string pdu) throw(GsmException);

    // encode pdu, return hexadecimal pdu string
    virtual string encode();

    // create textual representation of SMS
    virtual string toString() const;

    // inherited from SMSMessage
    Address address() const;
    Ref<SMSMessage> clone();

    // accessor functions
    Timestamp serviceCentreTimestamp() const {return _serviceCentreTimestamp;}
    bool protocolIdentifierPresent() const {return _protocolIdentifierPresent;}
    bool dataCodingSchemePresent() const {return _dataCodingSchemePresent;}
    bool userDataLengthPresent() const {return _userDataLengthPresent;}
    unsigned char protocolIdentifier() const
      {assert(_protocolIdentifierPresent); return _protocolIdentifier;}
    DataCodingScheme dataCodingScheme() const
      {assert(_dataCodingSchemePresent); return _dataCodingScheme;}
    UserDataHeader userDataHeader() const
      {assert(_userDataLengthPresent); return _userDataHeader;}
    string userData() const
      {assert(_userDataLengthPresent); return _userData;}

    void setServiceCentreTimestamp(Timestamp &x) {_serviceCentreTimestamp = x;}
    void setProtocolIdentifier(unsigned char x)
      {_protocolIdentifierPresent = true; _protocolIdentifier = x;}
    void setDataCodingScheme(DataCodingScheme x)
      {_dataCodingSchemePresent = true; _dataCodingScheme = x;}
    void setUserDataHeader(UserDataHeader x)
    {
      _userDataLengthPresent = true;
      _userDataHeader = x;
    }
    void setUserData(string x)
    {
      _userDataLengthPresent = true;
      _userData = x;
    }
    virtual ~SMSSubmitReportMessage() {}
  };

  // some useful typdefs
  typedef Ref<SMSMessage> SMSMessageRef;
};

#endif // GSM_SMS_H
