// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_parser.h
// *
// * Purpose: Parser to parse MA/TA result strings
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 13.5.1999
// *************************************************************************

#ifndef GSM_PARSER_H
#define GSM_PARSER_H

#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_error.h>
#include <string>
#include <vector>

using namespace std;

namespace gsmlib
{
  class Parser : public RefBase
  {
  private:
    unsigned int _i;            // index into _s, next character
    string _s;                  // string to parse
    bool _eos;                  // true if end-of-string reached in nextChar()

    // return next character or -1 if end of string
    int nextChar(bool skipWhiteSpace = true);
    
    // "puts back" a character
    void putBackChar() {if (! _eos) --_i;}

    // check for empty parameter (ie. "," or end of string)
    // skips white space
    // returns true if no parameter
    // or throw an GsmException if allowNoParameter == false
    bool checkEmptyParameter(bool allowNoParameter) throw(GsmException);
    
    // parse a string (like "string")
    // throw an exception if not well-formed
    string parseString2(bool stringWithQuotationMarks) throw(GsmException);

    // parse a int (like 1234)
    // throw an exception if not well-formed
    int parseInt2() throw(GsmException);

    // throw a parser exception
    void throwParseException(string message = "") throw(GsmException);

  public:
    Parser(string s);
    
    // the following functions skip white space
    // parse a character, if absent throw a GsmException
    // return false if allowNoChar == true and character not encountered
    bool parseChar(char c, bool allowNoChar = false) throw(GsmException);

    // parse a list of the form "("ABC", DEF")"
    // the list can be empty (ie. == "" ) if allowNoList == true
    vector<string> parseStringList(bool allowNoList = false)
      throw(GsmException);

    // parse a list of the form "(12, 14)" or "(1-4, 10)"
    // the result is returned as a bit vector where for each integer
    // in the list and/or range(s) a bit is set
    // the list can be empty (ie. == "") if allowNoList == true
    vector<bool> parseIntList(bool allowNoList = false)
      throw(GsmException);

    // parse a list of parameter ranges (see below)
    // the list can be empty (ie. == "" ) if allowNoList == true
    vector<ParameterRange> parseParameterRangeList(bool allowNoList = false)
      throw(GsmException);

    // parse a string plus its valid integer range of the
    // form "("string",(1-125))"
    // the parameter range may be absent if allowNoParameterRange == true
    ParameterRange parseParameterRange(bool allowNoParameterRange = false)
      throw(GsmException);

    // parse an integer range of the form "(1-125)"
    // the range may be absent if allowNoRange == true
    // then IntRange::_high and _low are set to NOT_SET
    // the range may be short if allowNonRange == true
    // then IntRange::_high is set to NOT_SET
    IntRange parseRange(bool allowNoRange = false, bool allowNonRange = false)
      throw(GsmException);
    
    // parse an integer of the form "1234"
    // allow absent int if allowNoInt == true
    // then it returns NOT_SET
    int parseInt(bool allowNoInt = false) throw(GsmException);

    // parse a string of the form ""string""
    // allow absent string if allowNoString == true
    // then it returns ""
    // if stringWithQuotationMarks == true the string may contain """
    // the string is then parsed till the end of the line
    string parseString(bool allowNoString = false,
                       bool stringWithQuotationMarks = false)
      throw(GsmException);

    // parse a single ","
    // the comma may be absent if allowNoComma == true
    // returns true if there was a comma
    bool parseComma(bool allowNoComma = false) throw(GsmException);
    
    // parse till end of line, return result without whitespace
    string parseEol() throw(GsmException);

    // check that end of line is reached
    void checkEol() throw(GsmException);

    // return string till end of line without whitespace
    // (does not change internal state)
    string getEol();
  };
};

#endif // GSM_PARSER_H
