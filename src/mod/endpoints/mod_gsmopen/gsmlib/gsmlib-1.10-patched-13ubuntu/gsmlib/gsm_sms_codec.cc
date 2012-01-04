// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sms_codec.cc
// *
// * Purpose: Coder and Encoder for SMS TPDUs
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 17.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_sysdep.h>
#include <gsmlib/gsm_sms_codec.h>
#include <gsmlib/gsm_util.h>
#include <time.h>
#include <strstream>
#include <iomanip>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <climits>
#include <string>
using namespace std;
using namespace gsmlib;

// Address members

Address::Address(string number) : _plan(ISDN_Telephone)
{
  number = removeWhiteSpace(number);
  if (number.length() > 0 && number[0] == '+')
  {
    _type = International;
    _number = number.substr(1, number.length() - 1);
  }
  else
  {
    _type = Unknown;
    _number = number;
  }
}

string Address::toString() const
{
  if (_type == International)
    return "+" + _number;
  else
    return _number;
}

bool gsmlib::operator<(const Address &x, const Address &y)
{
  // normalize numbers according to the following two rules:
  // - prepend "+" if international number
  // - append 0s to the shorter number so that both numbers have equal length
  string xnumber = x._number;
  string ynumber = y._number;
  static string twenty0s = "00000000000000000000";

  if (x._type == Address::International) xnumber = "+" + xnumber;
  if (y._type == Address::International) ynumber = "+" + ynumber;
  
  while (xnumber.length() != ynumber.length())
    if (xnumber.length() < ynumber.length())
    {
      int diff = ynumber.length() - xnumber.length();
      xnumber += twenty0s.substr(0, (diff > 20 ? 20 : diff));
    }
    else
    {
      int diff = xnumber.length() - ynumber.length();
      ynumber += twenty0s.substr(0, (diff > 20 ? 20 : diff));
    }

  return xnumber < ynumber;
}

bool gsmlib::operator==(const Address &x, const Address &y)
{
  return x._number == y._number && x._plan == y._plan;
}

// Timestamp members

bool Timestamp::empty() const
{
  return _year == 0 && _month == 0 && _day == 0 && _hour == 0 && 
    _minute == 0 && _seconds == 0 && _timeZoneMinutes == 0;
}

string Timestamp::toString(bool appendTimeZone) const
{
  short timeZoneMinutes = _timeZoneMinutes;
  short timeZoneHours = timeZoneMinutes / 60;
  timeZoneMinutes %= 60;

  // format date and time in a locale-specific way
  struct tm t;
  t.tm_sec = _seconds;
  t.tm_min = _minute;
  t.tm_hour = _hour;
  t.tm_mon = _month - 1;
  // year 2000 heuristics, SMSs cannot be older than start of GSM network
  t.tm_year = _year < 80 ? _year + 100 : _year;
  t.tm_mday = _day;
  t.tm_isdst = -1;
  t.tm_yday = 0;
  t.tm_wday = 0;
  
#ifdef BROKEN_STRFTIME
  char formattedTime[1024];
  strftime(formattedTime, 1024, "%x %X", &t);
#else
  int formattedTimeSize = strftime(NULL, INT_MAX, "%x %X", &t) + 1;
  char *formattedTime = (char*)alloca(sizeof(char) * formattedTimeSize);
  strftime(formattedTime, formattedTimeSize, "%x %X", &t);
#endif

  if (! appendTimeZone)
    return formattedTime;

  ostrstream os;
  os << formattedTime << " (" << (_negativeTimeZone ? '-' : '+')
     << setfill('0') << setw(2) << timeZoneHours 
     << setw(2) << timeZoneMinutes << ')' << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result;
}

bool gsmlib::operator<(const Timestamp &x, const Timestamp &y)
{
  // we don't take time zone info into account because
  // - it's more complicated to compute
  // - it might confuse the user for whom it's also too complicated
  if (x._year < y._year)
    return true;
  else if (x._year > y._year)
    return false;

  if (x._month < y._month)
    return true;
  else if (x._month > y._month)
    return false;

  if (x._day < y._day)
    return true;
  else if (x._day > y._day)
    return false;

  if (x._hour < y._hour)
    return true;
  else if (x._hour > y._hour)
    return false;

  if (x._minute < y._minute)
    return true;
  else if (x._minute > y._minute)
    return false;

  return x._seconds < y._seconds;
}

bool gsmlib::operator==(const Timestamp &x, const Timestamp &y)
{
  // we don't take time zone info in order to be consistent with operator<
  return x._year == y._year && x._month == y._month && x._day == y._day &&
    x._hour == y._hour && x._minute == y._minute && x._seconds == y._seconds;
}

// TimePeriod members

string TimePeriod::toString() const
{
  switch (_format)
  {
  case NotPresent:
    return _("not present");
  case Relative:
  {
    ostrstream os;
    if (_relativeTime <= 143)
      os << ((int)_relativeTime + 1) * 5 << _(" minutes");
    else if (_relativeTime <= 167)
      os << 12 * 60 + ((int)_relativeTime - 143) * 30 << _(" minutes");
    else if (_relativeTime <= 196)
      os << (int)_relativeTime - 166 << _(" days");
    else if (_relativeTime <= 143)
      os << (int)_relativeTime - 192 << _(" weeks");
    os << ends;
    char *ss = os.str();
    string result(ss);
    delete[] ss;
    return result;
  }
  case Absolute:
    return _absoluteTime.toString();
  default:
    return _("unknown");
  }
}

// DataCodingScheme members

string DataCodingScheme::toString() const
{
  string result;
  if (compressed()) result += _("compressed   ");
  if (messageWaitingIndication())
    switch (getMessageWaitingType())
    {
    case DCS_VOICEMAIL_MESSAGE_WAITING:
      result += _("voicemail message waiting");
      break;
    case DCS_FAX_MESSAGE_WAITING:
      result += _("fax message waiting");
      break;
    case DCS_ELECTRONIC_MAIL_MESSAGE_WAITING:
      result += _("electronic mail message waiting");
      break;
    case DCS_OTHER_MESSAGE_WAITING:
      result += _("other message waiting");
      break;
    }
  else
    switch (getAlphabet())
    {
    case DCS_DEFAULT_ALPHABET:
      result += _("default alphabet");
      break;
    case DCS_EIGHT_BIT_ALPHABET:
      result += _("8-bit alphabet");
      break;
    case DCS_SIXTEEN_BIT_ALPHABET:
      result += _("16-bit alphabet");
      break;
    case DCS_RESERVED_ALPHABET:
      result += _("reserved alphabet");
      break;
    }
  return result;
}

// SMSDecoder members

SMSDecoder::SMSDecoder(string pdu) : _bi(0), _septetStart(NULL)
{
  _p = new unsigned char[pdu.length() / 2];
  _op = _p;
  if (! hexToBuf(pdu, _p))
    throw GsmException(_("bad hexadecimal PDU format"), SMSFormatError);
  _maxop = _op + pdu.length() / 2;
}

void SMSDecoder::alignOctet()
{
  if (_bi != 0)
  {
    _bi = 0;
    ++_op;
  }
}
    
void SMSDecoder::alignSeptet()
{
  assert(_septetStart != NULL);
  while (((_op - _septetStart) * 8 + _bi) % 7 != 0) getBit();
}
    
unsigned char SMSDecoder::get2Bits()
{
  unsigned char result = getBit();
  return result | (getBit() << 1);
}

unsigned char SMSDecoder::getOctet()
{
  alignOctet();
  if (_op >= _maxop)
    throw GsmException(_("premature end of PDU"), SMSFormatError);
  return *_op++;
}

void SMSDecoder::getOctets(unsigned char* octets, unsigned short length)
{
  alignOctet();
  for (unsigned short i = 0; i < length; ++i)
  {
    if (_op >= _maxop)
      throw GsmException(_("premature end of PDU"), SMSFormatError);
    *octets++ = *_op++;
  }
}

string SMSDecoder::getSemiOctets(unsigned short length)
{
  string result;
  result.reserve(length);
  alignOctet();
  for (unsigned short i = 0; i < length; ++i)
  {
    if (_bi == 0)
    {
      if (_op >= _maxop)
        throw GsmException(_("premature end of PDU"), SMSFormatError);
      // bits 0..3 are most significant
      result += '0' + (*_op & 0xf);
      _bi = 4;
    }
    else
    {
      if (_op >= _maxop)
        throw GsmException(_("premature end of PDU"), SMSFormatError);
      // bits 4..7 are least significant, skip 0xf digit
      if ((*_op & 0xf0) != 0xf0)
        result += '0' + (*_op >> 4);
      _bi = 0;
      ++_op;
    }
  }
  alignOctet();
  return result;
}

unsigned long SMSDecoder::getSemiOctetsInteger(unsigned short length)
{
  unsigned long result = 0;
  alignOctet();
  for (unsigned short i = 0; i < length; ++i)
  {
    if (_bi == 0)
    {
      if (_op >= _maxop)
        throw GsmException(_("premature end of PDU"), SMSFormatError);
      // bits 0..3 are most significant
      result = result * 10 + (*_op & 0xf);
      _bi = 4;
    }
    else
    {
      if (_op >= _maxop)
        throw GsmException(_("premature end of PDU"), SMSFormatError);
      // bits 4..7 are least significant, skip 0xf digit
      if ((*_op & 0xf0) != 0xf0)
        result = result * 10 + (*_op >> 4);
      _bi = 0;
      ++_op;
    }
  }
  alignOctet();
  return result;
}

unsigned long SMSDecoder::getTimeZone(bool &negativeTimeZone)
{
  unsigned long result = 0;
  alignOctet();
  for (unsigned short i = 0; i < 2; ++i)
  {
    if (_bi == 0)
    {
      if (_op >= _maxop)
        throw GsmException(_("premature end of PDU"), SMSFormatError);
      // bits 0..3 are most significant
      if (i == 0)
      {                         // get sign
        result = result * 10 + (*_op & 0x7);
        negativeTimeZone = (*_op & 0x8 == 0);
      }
      else
        result = result * 10 + (*_op & 0xf);
      _bi = 4;
    }
    else
    {
      if (_op >= _maxop)
        throw GsmException(_("premature end of PDU"), SMSFormatError);
      // bits 4..7 are least significant
      result = result * 10 + (*_op >> 4);
      _bi = 0;
      ++_op;
    }
  }
  alignOctet();
  return result * 15;           // compute minutes
}

unsigned long SMSDecoder::getInteger(unsigned short length)
{
  unsigned long result = 0;
  for (unsigned short i = 0; i < length; ++i)
    result |= (getBit() << i);
  return result;
}

string SMSDecoder::getString(unsigned short length)
{
  string result;
  alignSeptet();
  for (unsigned short i = 0; i < length; ++i)
  {
    unsigned char c = 0;
    for (unsigned short j = 0; j < 7; ++j)
      c |= getBit() << j;
    result += c;
  }
  return result;
}

Address SMSDecoder::getAddress(bool scAddressFormat)
{
  Address result;
  alignOctet();

  unsigned char addressLength = getOctet();
  if (addressLength == 0 && scAddressFormat)
    return result; // special case for SUBMIT-PDUs

  // parse Type-of-Address
  result._plan = (Address::NumberingPlan)getInteger(4);
  result._type = (Address::Type)getInteger(3);

  // get address
  if (result._type == Address::Alphanumeric)
  {
    markSeptet();
    // addressLength is number of semi-octets
    // (addressLength / 2) * 8 is number of available bits
    // divided by 7 is number of 7-bit characters
    result._number = gsmToLatin1(getString(addressLength * 4 / 7));
    alignOctet();
  }
  else
    result._number = getSemiOctets(scAddressFormat ?
                                   (addressLength - 1) * 2 : addressLength);
  return result;
}

Timestamp SMSDecoder::getTimestamp()
{
  Timestamp result;

  result._year = getSemiOctetsInteger(2);
  result._month = getSemiOctetsInteger(2);
  result._day = getSemiOctetsInteger(2);
  result._hour = getSemiOctetsInteger(2);
  result._minute = getSemiOctetsInteger(2);
  result._seconds = getSemiOctetsInteger(2);
  result._timeZoneMinutes = getTimeZone(result._negativeTimeZone);
  return result;
}

TimePeriod SMSDecoder::getTimePeriod(TimePeriod::Format format)
{
  TimePeriod result;
  result._format = format;
  switch (format)
  {
  case TimePeriod::NotPresent:
    break;
  case TimePeriod::Relative:
    result._relativeTime = getOctet();
    break;
  case TimePeriod::Absolute:
    result._absoluteTime = getTimestamp();
    break;
  default:
    throw GsmException(_("unknown time period format"), SMSFormatError);
    break;
  }
  return result;
}

SMSDecoder::~SMSDecoder()
{
  delete _p;
}

// SMSEncoder members

SMSEncoder::SMSEncoder() : _bi(0), _op(_p)
{
  memset((void*)_p, 0, sizeof(_p));
}

void SMSEncoder::alignOctet()
{
  if (_bi != 0)
  {
    _bi = 0;
    ++_op;
  }
}
    
void SMSEncoder::alignSeptet()
{
  while (((_op - _septetStart) * 8 + _bi) % 7 != 0) setBit();
}
    
void SMSEncoder::set2Bits(unsigned char twoBits)
{
  setBit(twoBits & 1);
  setBit((twoBits & 2) == 2);
}

void SMSEncoder::setOctet(unsigned char octet)
{
  alignOctet();
  *_op++ = octet;
}

void SMSEncoder::setOctets(const unsigned char* octets, unsigned short length)
{
  alignOctet();
  for (unsigned short i = 0; i < length; ++i)
    *_op++ = octets[i];
}

void SMSEncoder::setSemiOctets(string semiOctets)
{
  alignOctet();
  for (unsigned int i = 0; i < semiOctets.length(); ++i)
  {
    if (_bi == 0)
    {
      *_op = semiOctets[i] - '0';
      _bi = 4;
    }
    else
    {
      *_op++ |= (semiOctets[i] - '0') << 4;
      _bi = 0;
    }
  }
  if (_bi == 4)
    *_op++ |= 0xf0;
  _bi = 0;
}

void SMSEncoder::setSemiOctetsInteger(unsigned long intValue,
                                      unsigned short length)
{
  ostrstream os;
  os << intValue << ends;
  char *ss = os.str();
  string s(ss);
  delete[] ss;
  assert(s.length() <= length);
  while (s.length() < length) s = '0' + s;
  setSemiOctets(s);
}

void SMSEncoder::setTimeZone(bool negativeTimeZone, unsigned long timeZone)
{
  setSemiOctetsInteger(timeZone / 15, 2);
  if (!negativeTimeZone)
    *(_op - 1) |= 8;
}

void SMSEncoder::setInteger(unsigned long intvalue, unsigned short length)
{
  for (unsigned short i = 0; i < length; ++i)
    setBit((intvalue & (1 << i)) != 0);
}

void SMSEncoder::setString(string stringValue)
{
  alignSeptet();
  for (unsigned int i = 0; i < stringValue.length(); ++i)
  {
    unsigned char c = stringValue[i];
    for (unsigned short j = 0; j < 7; ++j)
      setBit(((1 << j) & c) != 0);
  }
}

void SMSEncoder::setAddress(Address &address, bool scAddressFormat)
{
  alignOctet();
  if (scAddressFormat)
  {
    unsigned int numberLen = address._number.length();
    if (numberLen == 0)
    {
      setOctet(0);              // special case: use default SC address
      return;                   // (set by +CSCA=)
    }
    setOctet(numberLen / 2 + numberLen % 2 + 1);
    // not supported for SCA format
    assert(address._type != Address::Alphanumeric);
  }
  else
    if (address._type == Address::Alphanumeric)
      // address in GSM default encoding, see also comment in getAddress()
      setOctet((address._number.length() * 7 + 6) / 8 * 2);
    else
      setOctet(address._number.length());

  setInteger(address._plan, 4);
  setInteger(address._type, 3);
  setBit(1);
  
  if (address._number.length() > 0)
    if (address._type == Address::Alphanumeric)
    {
      markSeptet();
      setString(latin1ToGsm(address._number));
    }
    else
      setSemiOctets(address._number);
  alignOctet();
}

void SMSEncoder::setTimestamp(Timestamp timestamp)
{
  setSemiOctetsInteger(timestamp._year, 2);
  setSemiOctetsInteger(timestamp._month, 2);
  setSemiOctetsInteger(timestamp._day, 2);
  setSemiOctetsInteger(timestamp._hour, 2);
  setSemiOctetsInteger(timestamp._minute, 2);
  setSemiOctetsInteger(timestamp._seconds, 2);
  setTimeZone(timestamp._negativeTimeZone, timestamp._timeZoneMinutes);
}

void SMSEncoder::setTimePeriod(TimePeriod period)
{
  switch (period._format)
  {
  case TimePeriod::NotPresent:
    break;
  case TimePeriod::Relative:
    setOctet(period._relativeTime);
    break;
  case TimePeriod::Absolute:
    setTimestamp(period._absoluteTime);
    break;
  default:
    assert(0);
    break;
  }
}

string SMSEncoder::getHexString()
{
  short bi = _bi;
  unsigned char *op = _op;
  alignOctet();
  string result = bufToHex(_p, _op - _p);
  _bi = bi;
  _op = op;
  return result;
}

unsigned int SMSEncoder::getLength()
{
  short bi = _bi;
  unsigned char *op = _op;
  alignOctet();
  unsigned int result = _op - _p;
  _bi = bi;
  _op = op;
  return result;
}

// UserDataHeader members

void UserDataHeader::encode(SMSEncoder &e)
{
  e.setOctet(_udh.length());
  e.setOctets((unsigned char*)_udh.data(), _udh.length());
}

void UserDataHeader::decode(SMSDecoder &d)
{
  unsigned char udhLen = d.getOctet();
  unsigned char *s =
    (unsigned char*)alloca(sizeof(unsigned char) * udhLen);
  d.getOctets(s, udhLen);
  string ss((char*)s, (unsigned int)udhLen);
  _udh = ss;
}

string UserDataHeader::getIE(unsigned char id)
{
  int udhl, pos = 0;
	
  udhl = _udh.length();
  while (pos < udhl)
  {
    unsigned char iei = _udh[pos++];
    unsigned char ieidl = _udh[pos++];
    if (iei == id) return _udh.substr(pos, ieidl);
    pos += ieidl;
  }
  return "";
}
