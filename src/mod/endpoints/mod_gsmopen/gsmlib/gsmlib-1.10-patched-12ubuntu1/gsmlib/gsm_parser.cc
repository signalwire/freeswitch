// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_parser.cc
// *
// * Purpose: Parser to parse MA/TA result strings
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 13.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_parser.h>
#include <gsmlib/gsm_nls.h>
#include <ctype.h>
#include <assert.h>
#include <strstream>

using namespace std;
using namespace gsmlib;

// Parser members

int Parser::nextChar(bool skipWhiteSpace)
{
  if (skipWhiteSpace)
    while (_i < _s.length() && isspace(_s[_i])) ++_i;

  if (_i == _s.length())
  {
    _eos = true;
    return -1;
  }

  return _s[_i++];
}
    
bool Parser::checkEmptyParameter(bool allowNoParameter) throw(GsmException)
{
  int c = nextChar();
  if (c == ',' || c == -1)
    if (allowNoParameter)
    {
      putBackChar();
      return true;
    }
    else
      throwParseException(_("expected parameter"));

  putBackChar();
  return false;
}
    
string Parser::parseString2(bool stringWithQuotationMarks)
  throw(GsmException)
{
  int c;
  string result;
  if (parseChar('"', true))  // OK, string starts and ends with quotation mark
    if (stringWithQuotationMarks)
    {
      // read till end of line
      while ((c = nextChar(false)) != -1)
        result += c;
      
      // check for """ at end of line
      if (result.length() == 0 || result[result.length() - 1]  != '"')
        throwParseException(_("expected '\"'"));
      
      // remove """ at the end
      result.resize(result.length() - 1);
    }
    else
    {
      // read till next """
      while ((c = nextChar(false)) != '"')
        if (c == -1)
          throwParseException();
        else
          result += c;
    }
  else                          // string ends with "," or EOL
  {
    c = nextChar(false);
    while (c != ',' && c != -1)
    {
      result += c;
      c = nextChar(false);
    }
    if (c == ',') putBackChar();
  }
    
  return result;
}

int Parser::parseInt2() throw(GsmException)
{
  string s;
  int c;
  int result;

  while (isdigit(c = nextChar())) s += c;

  putBackChar();
  if (s.length() == 0)
    throwParseException(_("expected number"));

  istrstream is(s.c_str());
  is >> result;
  return result;
}

void Parser::throwParseException(string message) throw(GsmException)
{
  ostrstream os;
  if (message.length() == 0)
    throw GsmException(stringPrintf(_("unexpected end of string '%s'"),
                                    _s.c_str()), ParserError);
  else
    throw GsmException(message +
                       stringPrintf(_(" (at position %d of string '%s')"), _i,
                                    _s.c_str()), ParserError);
}

Parser::Parser(string s) : _i(0), _s(s), _eos(false)
{
}

bool Parser::parseChar(char c, bool allowNoChar) throw(GsmException)
{
  if (nextChar() != c)
    if (allowNoChar)
    {
      putBackChar();
      return false;
    }
    else
      throwParseException(stringPrintf(_("expected '%c'"), c));
  return true;
}

vector<string> Parser::parseStringList(bool allowNoList)
  throw(GsmException)
{
  // handle case of empty parameter
  vector<string> result;
  if (checkEmptyParameter(allowNoList)) return result;

  parseChar('(');
  if (nextChar() != ')')
  {
    putBackChar();
    while (1)
    {
      result.push_back(parseString());
      int c = nextChar();
      if (c == ')')
      break;
      if (c == -1)
        throwParseException();
      if (c != ',')
        throwParseException(_("expected ')' or ','"));
    }
  }
  
  return result;
}

vector<bool> Parser::parseIntList(bool allowNoList)
  throw(GsmException)
{
  // handle case of empty parameter
  bool isRange = false;
  vector<bool> result;
  int resultCapacity = 0;
  unsigned int saveI = _i;

  if (checkEmptyParameter(allowNoList)) return result;

  // check for the case of a integer list consisting of only one parameter
  // some TAs omit the parentheses in this case
  if (isdigit(nextChar()))
  {
    putBackChar();
    int num = parseInt();
    result.resize(num + 1, false);
    result[num] = true;
    return result;
  }
  putBackChar();

  // run in two passes
  // pass 0: find capacity needed for result
  // pass 1: resize result and fill it in
  for (int pass = 0; pass < 2; ++pass)
  {
    if (pass == 1)
    {
      _i = saveI;
      result.resize(resultCapacity + 1, false);
    }

    parseChar('(');
    if (nextChar() != ')')
    {
      putBackChar();
      int lastInt = -1;
      while (1)
      {
        int thisInt = parseInt();

        if (isRange)
        {
          assert(lastInt != -1);
          if (lastInt <= thisInt)
            for (int i = lastInt; i < thisInt; ++i)
            {
              if (i > resultCapacity)
                resultCapacity = i;
              if (pass == 1)
                result[i] = true;
            }
          else
            for (int i = thisInt; i < lastInt; ++i)
            {
              if (i > resultCapacity)
                resultCapacity = i;
              if (pass == 1)
                result[i] = true;
            }
          isRange = false;
        }

        if (thisInt > resultCapacity)
          resultCapacity = thisInt;
        if (pass == 1)
          result[thisInt] = true;
        lastInt = thisInt;
      
        int c = nextChar();
        if (c == ')')
          break;

        if (c == -1)
          throwParseException();

        if (c != ',' && c != '-')
          throwParseException(_("expected ')', ',' or '-'"));

        if (c == ',')
          isRange = false;
        else                      // is '-'
          if (isRange)
            throwParseException(_("range of the form a-b-c not allowed"));
          else
            isRange = true;
      }
    }
  }
  if (isRange)
    throwParseException(_("range of the form a- no allowed"));
  return result;
}

vector<ParameterRange> Parser::parseParameterRangeList(bool allowNoList)
  throw(GsmException)
{
  // handle case of empty parameter
  vector<ParameterRange> result;
  if (checkEmptyParameter(allowNoList)) return result;

  result.push_back(parseParameterRange());
  while (parseComma(true))
  {
    result.push_back(parseParameterRange());
  }
  
  return result;
}

ParameterRange Parser::parseParameterRange(bool allowNoParameterRange)
  throw(GsmException)
{
  // handle case of empty parameter
  ParameterRange result;
  if (checkEmptyParameter(allowNoParameterRange)) return result;

  parseChar('(');
  result._parameter = parseString();
  parseComma();
  result._range = parseRange(false, true);
  parseChar(')');

  return result;
}

IntRange Parser::parseRange(bool allowNoRange, bool allowNonRange)
  throw(GsmException)
{
  // handle case of empty parameter
  IntRange result;
  if (checkEmptyParameter(allowNoRange)) return result;

  parseChar('(');
  result._low = parseInt();
  // allow non-ranges is allowNonRange == true
  if (parseChar('-', allowNonRange))
    result._high = parseInt();
  parseChar(')');

  return result;
}

int Parser::parseInt(bool allowNoInt) throw(GsmException)
{
  // handle case of empty parameter
  int result = NOT_SET;
  if (checkEmptyParameter(allowNoInt)) return result;

  result = parseInt2();

  return result;
}

string Parser::parseString(bool allowNoString,
                           bool stringWithQuotationMarks)
  throw(GsmException)
{
  // handle case of empty parameter
  string result;
  if (checkEmptyParameter(allowNoString)) return result;

  result = parseString2(stringWithQuotationMarks);

  return result;
}

bool Parser::parseComma(bool allowNoComma) throw(GsmException)
{
  if (nextChar() != ',')
    if(allowNoComma)
    {
      putBackChar();
      return false;
    }
    else
      throwParseException(_("expected comma"));
  return true;
}

string Parser::parseEol() throw(GsmException)
{
  string result;
  int c;
  while ((c = nextChar()) != -1) result += c;
  return result;
}

void Parser::checkEol() throw(GsmException)
{
  if (nextChar() != -1)
  {
    putBackChar();
    throwParseException(_("expected end of line"));
  }
}

string Parser::getEol()
{
  string result;
  int c;
  unsigned int saveI = _i;
  bool saveEos = _eos;
  while ((c = nextChar()) != -1) result += c;
  _i = saveI;
  _eos = saveEos;
  return result;
}
