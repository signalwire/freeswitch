// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sie_me.cc
// *
// * Purpose: Mobile Equipment/Terminal Adapter and SMS functions
// *          (According to "AT command set for S45 Siemens mobile phones"
// *           v1.8, 26. July 2001 - Common AT prefix is "^S")
// *
// * Author:  Christian W. Zuckschwerdt  <zany@triq.net>
// *
// * Created: 2001-12-15
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_parser.h>
#include <gsmlib/gsm_util.h>
#include <gsm_sie_me.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

// SieMe members

void SieMe::init() throw(GsmException)
{
}

SieMe::SieMe(Ref<Port> port) throw(GsmException) : MeTa::MeTa(port)
{
  // initialize Siemens ME

  init();
}

vector<string> SieMe::getSupportedPhonebooks() throw(GsmException)
{
  Parser p(_at->chat("^SPBS=?", "^SPBS:"));
  return p.parseStringList();
}

string SieMe::getCurrentPhonebook() throw(GsmException)
{
  if (_lastPhonebookName == "")
  {
    Parser p(_at->chat("^SPBS?", "^SPBS:"));
    // answer is e.g. ^SPBS: "SM",41,250
    _lastPhonebookName = p.parseString();
    p.parseComma();
    int _currentNumberOfEntries = p.parseInt();
    p.parseComma();
    int _maxNumberOfEntries = p.parseInt();
  }
  return _lastPhonebookName;
}

void SieMe::setPhonebook(string phonebookName) throw(GsmException)
{
  if (phonebookName != _lastPhonebookName)
  {
    _at->chat("^SPBS=\"" + phonebookName + "\"");
    _lastPhonebookName = phonebookName;
  }
}


IntRange SieMe:: getSupportedSignalTones() throw(GsmException)
{
  Parser p(_at->chat("^SPST=?", "^SPST:"));
  // ^SPST: (0-4),(0,1)
  IntRange typeRange = p.parseRange();
  p.parseComma();
  vector<bool> volumeList = p.parseIntList();
  return typeRange;
}

void SieMe:: playSignalTone(int tone) throw(GsmException)
{
  _at->chat("^SPST=" + intToStr(tone) + ",1");
}

void SieMe:: stopSignalTone(int tone) throw(GsmException)
{
  _at->chat("^SPST=" + intToStr(tone) + ",0");
}


IntRange SieMe::getSupportedRingingTones() throw(GsmException) // (AT^SRTC=?)
{
  Parser p(_at->chat("^SRTC=?", "^SRTC:"));
  // ^SRTC: (0-42),(1-5)
  IntRange typeRange = p.parseRange();
  p.parseComma();
  IntRange volumeRange = p.parseRange();
  return typeRange;
}

int SieMe::getCurrentRingingTone() throw(GsmException) // (AT^SRTC?)
{
  Parser p(_at->chat("^SRTC?", "^SRTC:"));
  // ^SRTC: 41,2,0
  int type = p.parseInt();
  p.parseComma();
  int volume = p.parseInt();
  p.parseComma();
  int ringing = p.parseInt();
  return type;
}

void SieMe::setRingingTone(int tone, int volume) throw(GsmException)
{
  _at->chat("^SRTC=" + intToStr(tone) + "," + intToStr(volume));
}

void SieMe:: playRingingTone() throw(GsmException)
{
  // get ringing bool
  Parser p(_at->chat("^SRTC?", "^SRTC:"));
  // ^SRTC: 41,2,0
  int type = p.parseInt();
  p.parseComma();
  int volume = p.parseInt();
  p.parseComma();
  int ringing = p.parseInt();

  if (ringing == 0)
    toggleRingingTone();
}

void SieMe::stopRingingTone() throw(GsmException)
{
  // get ringing bool
  Parser p(_at->chat("^SRTC?", "^SRTC:"));
  // ^SRTC: 41,2,0
  int type = p.parseInt();
  p.parseComma();
  int volume = p.parseInt();
  p.parseComma();
  int ringing = p.parseInt();

  if (ringing == 1)
    toggleRingingTone();
}

void SieMe::toggleRingingTone() throw(GsmException) // (AT^SRTC)
{
  _at->chat("^SRTC");
}

// Siemens get supported binary read
vector<ParameterRange> SieMe::getSupportedBinaryReads() throw(GsmException)
{
  Parser p(_at->chat("^SBNR=?", "^SBNR:"));
  // ^SBNR: ("bmp",(0-3)),("mid",(0-4)),("vcf",(0-500)),("vcs",(0-50))

  return p.parseParameterRangeList();
}

// Siemens get supported binary write
vector<ParameterRange> SieMe::getSupportedBinaryWrites() throw(GsmException)
{
  Parser p(_at->chat("^SBNW=?", "^SBNW:"));
  // ^SBNW: ("bmp",(0-3)),("mid",(0-4)),("vcf",(0-500)),("vcs",(0-50)),("t9d",(0))

  return p.parseParameterRangeList();
}

// Siemens Binary Read
BinaryObject SieMe::getBinary(string type, int subtype) throw(GsmException)
{
  // expect several response lines
  vector<string> result;
  result = _at->chatv("^SBNR=\"" + type + "\"," + intToStr(subtype), "^SBNR:");
  // "bmp",0,1,5 <CR><LF> pdu <CR><LF> "bmp",0,2,5 <CR><LF> ...
  // most likely to be PDUs of 382 chars (191 * 2)
  string pdu;
  int fragmentCount = 0;
  for (vector<string>::iterator i = result.begin(); i != result.end(); ++i)
  {
    ++fragmentCount;
    // parse header
    Parser p(*i);
    string fragmentType = p.parseString();
    if (fragmentType != type)
      throw GsmException(_("bad PDU type"), ChatError);
    p.parseComma();
    int fragmentSubtype = p.parseInt();
    if (fragmentSubtype != subtype)
      throw GsmException(_("bad PDU subtype"), ChatError);
    p.parseComma();
    int fragmentNumber = p.parseInt();
    if (fragmentNumber != fragmentCount)
      throw GsmException(_("bad PDU number"), ChatError);
    p.parseComma();
    int numberOfFragments = p.parseInt();
    if (fragmentNumber > numberOfFragments)
      throw GsmException(_("bad PDU number"), ChatError);

    // concat pdu fragment
    ++i;
    pdu += *i;
  }

  BinaryObject bnr;
  bnr._type = type;
  bnr._subtype = subtype;
  bnr._size = pdu.length() / 2;
  bnr._data = new unsigned char[pdu.length() / 2];
  if (! hexToBuf(pdu, bnr._data))
    throw GsmException(_("bad hexadecimal PDU format"), ChatError);

  return bnr;
}

// Siemens Binary Write
void SieMe::setBinary(string type, int subtype, BinaryObject obj)
  throw(GsmException)
{
  if (obj._size <= 0)
    throw GsmException(_("bad object"), ParameterError);

  // Limitation: The maximum pdu size is 176 bytes (or 352 characters)
  // this should be a configurable field 
  int maxPDUsize = 176;
  int numberOfPDUs = (obj._size + maxPDUsize - 1) / maxPDUsize;
  unsigned char *p = obj._data;

  for (int i = 1; i <= numberOfPDUs; ++i)
  {
    // construct pdu
    int size = maxPDUsize;
    if (i == numberOfPDUs)
      size = obj._size - (numberOfPDUs - 1) * maxPDUsize;
    string pdu = bufToHex(p, size);
    p += size;

    cout << "processing " << i << " of " << numberOfPDUs
	 << " of " << size << " bytes." << endl;
    cout << "^SBNW=\"" + type + "\"," + intToStr(subtype) + ","
	+ intToStr(i) + "," + intToStr(numberOfPDUs) << endl;
    cout << pdu << endl;

    _at->sendPdu("^SBNW=\"" + type + "\"," + intToStr(subtype) + ","
		 + intToStr(i) + "," + intToStr(numberOfPDUs), "",
		 pdu, true);
    cout << "OK" << endl;
  }
}





