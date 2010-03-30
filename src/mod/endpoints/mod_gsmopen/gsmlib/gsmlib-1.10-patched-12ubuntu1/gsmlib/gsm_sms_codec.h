// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sms_codec.h
// *
// * Purpose: Coder and Encoder for SMS TPDUs
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 16.5.1999
// *************************************************************************

#ifndef GSM_SMS_CODEC_H
#define GSM_SMS_CODEC_H

#include <string>
#include <assert.h>

using namespace std;

namespace gsmlib
{
  // this struct represents a telephone number
  // usually _type == Unknown or International
  // and _number == ISDN_Telephone
  struct Address
  {
    enum Type {Unknown = 0, International = 1, National = 2,
               NetworkSpecific = 3, Subscriber = 4,
               Alphanumeric = 5, Abbreviated = 6, Reserved = 7};
    enum NumberingPlan {UnknownPlan = 0, ISDN_Telephone = 1,
                        Data = 3, Telex = 4, NationalPlan = 8,
                        PrivatePlan = 9, Ermes = 10, ReservedPlan = 15};
    Type _type;
    NumberingPlan _plan;
    string _number;    

    Address() : _type(Unknown), _plan(UnknownPlan) {}
    // the constructor sets _type and _plan to defaults
    // _plan == ISDN_Telephone
    // _type == International if number starts with "+"
    // _type == unknown otherwise
    // number must be of the form "+123456" or "123456"
    Address(string number);

    // return string representation
    string toString() const;

    friend bool operator<(const Address &x, const Address &y);
    friend bool operator==(const Address &x, const Address &y);
  };

  // compare two addresses
  extern bool operator<(const Address &x, const Address &y);
  extern bool operator==(const Address &x, const Address &y);

  // representation of a SMS timestamp
  struct Timestamp
  {
    short _year, _month, _day, _hour, _minute, _seconds, _timeZoneMinutes;
    bool _negativeTimeZone;

    Timestamp() : _year(0), _month(0), _day(0), _hour(0),
      _minute(0), _seconds(0), _timeZoneMinutes(0), _negativeTimeZone(false) {}

    // return true if the time stamp is empty (ie. contains only zeroes)
    bool empty() const;

    // return string representation
    string toString(bool appendTimeZone = true) const;

    friend bool operator<(const Timestamp &x, const Timestamp &y);
    friend bool operator==(const Timestamp &x, const Timestamp &y);
  };

  // compare two timestamps
  extern bool operator<(const Timestamp &x, const Timestamp &y);
  extern bool operator==(const Timestamp &x, const Timestamp &y);

  // representation of time period
  struct TimePeriod
  {
    // possible values for validity period format
    enum Format {NotPresent = 0, Relative = 2, Absolute = 3};
    Format _format;
    Timestamp _absoluteTime;
    unsigned char _relativeTime;

    TimePeriod() : _format(NotPresent), _relativeTime(0) {}

    // return string representation (already translated)
    string toString() const;
  };

  // representation of DataCodingScheme
  // the data coding scheme is described in detail in ETSI GSM 03.38, section 4
  const unsigned char DCS_COMPRESSED = 0x20; // bit 5
  
  const unsigned char DCS_DEFAULT_ALPHABET = 0 << 2; // bit 2..3 == 0
  const unsigned char DCS_EIGHT_BIT_ALPHABET = 1 << 2; // bit 2..3 == 01
  const unsigned char DCS_SIXTEEN_BIT_ALPHABET = 2 << 2; // bit 2..3 == 10
  const unsigned char DCS_RESERVED_ALPHABET = 3 << 2; // bit 2..3 == 11
  
  const unsigned char DCS_MESSAGE_WAITING_INDICATION = 0xc0; // bit 7..6 == 11
  const unsigned char DCS_VOICEMAIL_MESSAGE_WAITING = 0;
  const unsigned char DCS_FAX_MESSAGE_WAITING = 1;
  const unsigned char DCS_ELECTRONIC_MAIL_MESSAGE_WAITING = 2;
  const unsigned char DCS_OTHER_MESSAGE_WAITING = 3;

  class DataCodingScheme
  {
  private:
    unsigned char _dcs;

  public:
    // initialize with data coding scheme octet
    DataCodingScheme(unsigned char dcs) : _dcs(dcs) {}
    
    // set to default values (no message waiting, no message class indication,
    // default 7-bit alphabet)
    DataCodingScheme() : _dcs(DCS_DEFAULT_ALPHABET) {}

    // return type of alphabet used (if messageWaitingIndication == false)
    unsigned char getAlphabet() const {return _dcs & (3 << 2);}
    
    // return true if message compressed
    // (if messageWaitingIndication == false)
    bool compressed() const {return _dcs & DCS_COMPRESSED == DCS_COMPRESSED;}

    // return true if message waiting indication
    bool messageWaitingIndication() const
      {return _dcs & DCS_MESSAGE_WAITING_INDICATION == 
         DCS_MESSAGE_WAITING_INDICATION;}

    // return type of waiting message (if messageWaitingIndication == true)
    unsigned char getMessageWaitingType() const {return _dcs & 3;}

    // return string representation (already translated)
    string toString() const;

    operator unsigned char() const {return _dcs;}
  };

  // utility class facilitate SMS TPDU decoding
  class SMSDecoder
  {
  private:
    unsigned char *_p;          // buffer to hold pdu
    short _bi;                  // bit index (0..7)
    unsigned char *_op;         // current octet pointer
    unsigned char *_septetStart; // start of septet string

    unsigned char *_maxop;      // pointer to last byte after _p

  public:
    // initialize with a hexadecimal octet string containing SMS TPDU
    SMSDecoder(string pdu);

    // align to octet border
    void alignOctet();
    
    // remember starting point of septets (important for alignSeptet())
    void markSeptet() {alignOctet(); _septetStart = _op;}

    // align to septet border
    void alignSeptet();
    
    // get single bit
    bool getBit()
    {
      assert(_op < _maxop);
      bool result = ((*_op >> _bi) & 1);
      if (_bi == 7)
      {
        _bi = 0;
        ++_op;
      }
      else
        ++_bi;
      return result;
    }

    // get two bits
    unsigned char get2Bits();

    // get one octet
    unsigned char getOctet();

    // get string of octets of specified length
    void getOctets(unsigned char* octets, unsigned short length);

    // get length semi-octets (bcd-coded number) as ASCII string of numbers
    string getSemiOctets(unsigned short length);

    // get length semi-octets (bcd-coded number) as integer
    unsigned long getSemiOctetsInteger(unsigned short length);

    // get time zone (in minutes) and time zone sign
    unsigned long getTimeZone(bool &negativeTimeZone);

    // get integer with length number of bits
    unsigned long getInteger(unsigned short length);

    // get length number of alphanumeric 7-bit characters
    // markSeptet() must be called before this function
    string getString(unsigned short length);

    // get address/telephone number
    // service centre address has special format
    Address getAddress(bool scAddressFormat = false);

    // get Timestamp
    Timestamp getTimestamp();

    // get TimePeriod of given format
    TimePeriod getTimePeriod(TimePeriod::Format format);

    // destructor
    ~SMSDecoder();
  };

  // utility class for SMS TPDU encoding
  class SMSEncoder
  {
  private:
    unsigned char _p[2000];     // buffer to hold pdu (2000 should be enough)
    short _bi;                  // bit index (0..7)
    unsigned char *_op;         // current octet pointer
    unsigned char *_septetStart; // start of septet string

  public:
    // constructor
    SMSEncoder();

    // align to octet border
    void alignOctet();

    // remember starting point of septets (important for alignSeptet())
    void markSeptet() {alignOctet(); _septetStart = _op;}

    // align to septet border
    void alignSeptet();
    
    // set single bit
    void setBit(bool bit = false)
    {
      if (bit)
        *_op |= (1 << _bi);
      if (_bi == 7)
      {
        _bi = 0;
        ++_op;
      }
      else
        ++_bi;
    }

    // set two bits
    void set2Bits(unsigned char twoBits);

    // set one octet
    void setOctet(unsigned char octet);

    // set string of octets of specified length
    void setOctets(const unsigned char* octets, unsigned short length);

    // set semi-octets semiOctets (given as ASCII string of numbers)
    void setSemiOctets(string semiOctets);

    // set semi-octets (given as integer)
    void setSemiOctetsInteger(unsigned long intValue, unsigned short length);

    // set time zone (in minutes) and time zone sign
    void setTimeZone(bool negativeTimeZone, unsigned long timeZone);

    // set integer with length number of bits
    void setInteger(unsigned long intvalue, unsigned short length);

    // set alphanumeric 7-bit characters
    void setString(string stringValue);

    // set address/telephone number
    // service centre address has special format
    void setAddress(Address &address, bool scAddressFormat = false);

    // set Timestamp
    void setTimestamp(Timestamp timestamp);

    // set TimePeriod
    void setTimePeriod(TimePeriod period);

    // return constructed TPDU as hex-encoded string
    string getHexString();

    // return current length of TPDU 
    unsigned int getLength();
  };

  // class to handle user data header
  class UserDataHeader
  {
  private:
    string _udh;
    
  public:
    // empty user data header
    UserDataHeader() {}

    // initialize with user data header
    UserDataHeader (string udh) : _udh(udh) {}

    // encode header
    void encode(SMSEncoder &e);

    // decode header
    void decode(SMSDecoder &d);

    // return a given information element, if present, or an empty string
    string getIE(unsigned char id);
    
    // return the size of the header
    unsigned int length() const {return _udh.length();}

    // return user data header as octet string
    operator string() const {return _udh;}
  };
};

#endif // GSM_SMS_CODEC_H
