// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_cb.h
// *
// * Purpose: Cell Broadcast Message Implementation
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.8.2001
// *************************************************************************

#ifndef GSM_CB_H
#define GSM_CB_H

#include <gsmlib/gsm_sms_codec.h>
#include <gsmlib/gsm_util.h>
#include <string>

using namespace std;

namespace gsmlib
{
  // representation of DataCodingScheme
  // The data coding scheme is described in detail in ETSI GSM 03.38, section 5
  // This class reuses the DCS_* constants from DataCodingScheme in 
  // gsm_sms_codec

  class CBDataCodingScheme
  {
  public:
    enum Language {German = 0, English = 1, Italian = 2, French = 3,
                   Spanish = 4, Dutch = 5, Swedish = 6, Danish = 7,
                   Portuguese = 8, Finnish = 9, Norwegian = 10, Greek = 11,
                   Turkish = 12, Unknown = 1000};

  private:
    unsigned char _dcs;
    Language _language;

  public:
    // initialize with data coding scheme octet
    CBDataCodingScheme(unsigned char dcs);
    
    // default constructor
    CBDataCodingScheme() : _dcs(DCS_DEFAULT_ALPHABET), _language(English) {}

    // return language of CBM
    Language getLanguage() const {return _language;}

    // return compression level (if language == Unknown)
    bool compressed() const {return (_dcs & DCS_COMPRESSED) == DCS_COMPRESSED;}

    // return type of alphabet used
    // (DCS_DEFAULT_ALPHABET, DCS_EIGHT_BIT_ALPHABET, DCS_SIXTEEN_BIT_ALPHABET,
    // DCS_RESERVED_ALPHABET)
    unsigned char getAlphabet() const
      {return _language == Unknown ? _dcs & (3 << 2) : DCS_DEFAULT_ALPHABET;}

    // create textual representation of CB data coding scheme
    string toString() const;
  };

  // representation of Cell Broadcast message (CBM)
  // The CBM format is described in detail in ETSI GSM 03.41, section 9.3
  
  class CBMessage : public RefBase
  {
  public:
    enum GeographicalScope {CellWide, PLMNWide, LocationAreaWide,
                            CellWide2};

  private:
    // fields parsed from the CB TPDU
    GeographicalScope _geographicalScope;
    int _messageCode;
    int _updateNumber;
    int _messageIdentifier;
    CBDataCodingScheme _dataCodingScheme;
    int _totalPageNumber;
    int _currentPageNumber;
    string _data;

  public:
    // constructor with given pdu
    CBMessage(string pdu) throw(GsmException);

    // accessor functions
    GeographicalScope getGeographicalScope() const {return _geographicalScope;}
    int getMessageCode() const {return _messageCode;}
    int getUpdateNumber() const {return _updateNumber;}
    int getMessageIdentifier() const {return _messageIdentifier;}
    CBDataCodingScheme getDataCodingScheme() const {return _dataCodingScheme;}
    int getTotalPageNumber() const {return _totalPageNumber;}
    int getCurrentPageNumber() const {return _currentPageNumber;}
    string getData() const {return _data;}

    // create textual representation of CBM
    string toString() const;
  };

  // some useful typdefs
  typedef Ref<CBMessage> CBMessageRef;
};

#endif // GSM_CB_H
