// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_cb.cc
// *
// * Purpose: Cell Broadcast Message Implementation
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.8.2001
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_sysdep.h>
#include <gsmlib/gsm_cb.h>
#include <gsmlib/gsm_nls.h>
#include <strstream>

using namespace std;
using namespace gsmlib;

// local constants

static const string dashes =
"---------------------------------------------------------------------------";

// CBDataCodingScheme members

CBDataCodingScheme::CBDataCodingScheme(unsigned char dcs) : _dcs(dcs)
{
  if ((_dcs & 0xf0) <= 0x30)    // bits 7..4 in the range 0000..0011
    if ((_dcs & 0x30) == 0)
      _language = (Language)_dcs;
  else
    _language = Unknown;
}

string CBDataCodingScheme::toString() const
{
  string result;
  if (compressed()) result += _("compressed   ");
  switch (getLanguage())
  {
  case German:
    result += _("German");
    break;
  case English:
    result += _("English");
    break;
  case Italian:
    result += _("Italian");
    break;
  case French:
    result += _("French");
    break;
  case Spanish:
    result += _("Spanish");
    break;
  case Dutch:
    result += _("Dutch");
    break;
  case Swedish:
    result += _("Swedish");
    break;
  case Danish:
    result += _("Danish");
    break;
  case Portuguese:
    result += _("Portuguese");
    break;
  case Finnish:
    result += _("Finnish");
    break;
  case Norwegian:
    result += _("Norwegian");
    break;
  case Greek:
    result += _("Greek");
    break;
  case Turkish:
    result += _("Turkish");
    break;
  }
  result += "   ";
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

// CBMessage members

CBMessage::CBMessage(string pdu) throw(GsmException)
{
  SMSDecoder d(pdu);
  _messageCode = d.getInteger(6) << 4;
  _geographicalScope = (GeographicalScope)d.get2Bits();
  _updateNumber = d.getInteger(4);
  _messageCode |= d.getInteger(4);
  _messageIdentifier = d.getInteger(8) << 8;
  _messageIdentifier |= d.getInteger(8);
  _dataCodingScheme = CBDataCodingScheme(d.getOctet());
  _totalPageNumber = d.getInteger(4);
  _currentPageNumber = d.getInteger(4);

  // the values 82 and 93 come from ETSI GSM 03.41, section 9.3
  d.markSeptet();
  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
  {
    _data = d.getString(93);
    _data = gsmToLatin1(_data);
  }
  else
  {
    unsigned char *s = 
      (unsigned char*)alloca(sizeof(unsigned char) * 82);
    d.getOctets(s, 82);
    _data.assign((char*)s, (unsigned int)82);
  }
}

string CBMessage::toString() const
{
  ostrstream os;
  os << dashes << endl
     << _("Message type: CB") << endl
     << _("Geographical scope: ");
  switch (_geographicalScope)
  {
  case CellWide:
    os << "Cell wide" << endl;
    break;
  case PLMNWide:
    os << "PLMN wide" << endl;
    break;
  case LocationAreaWide:
    os << "Location area wide" << endl;
    break;
  case CellWide2:
    os << "Cell wide (2)" << endl;
    break;
  }
  // remove trailing \r characters for output
  string data = _data;
  string::iterator i;
  for (i = data.end(); i > data.begin() && *(i - 1) == '\r';
       --i);
  data.erase(i, data.end());

  os << _("Message Code: ") << _messageCode << endl
     << _("Update Number: ") << _updateNumber << endl
     << _("Message Identifer: ") << _messageIdentifier << endl
     << _("Data coding scheme: ") << _dataCodingScheme.toString() << endl
     << _("Total page number: ") << _totalPageNumber << endl
     << _("Current page number: ") << _currentPageNumber << endl
     << _("Data: '") << data << "'" << endl
     << dashes << endl << endl << ends;
  char *ss = os.str();
  string result(ss);
  delete[] ss;
  return result;
}
