// *************************************************************************
// * GSM TA/ME library
// *
// * File:    testparser.cc
// *
// * Purpose: Test AT result code parser
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 17.5.1999
// *************************************************************************

#include <gsmlib/gsm_parser.h>
#include <assert.h>
#include <algorithm>
#include <iostream>

using namespace std;
using namespace gsmlib;

void printIntList(vector<bool> &vb)
{
  cout << "(";
  int j = 0;
  bool first = true;
  for (vector<bool>::iterator i = vb.begin();
       i != vb.end(); ++i)
  {
    if (*i)
    {
      if (! first) cout << ",";
      cout << j;
      first = false;
    }
    ++j;
  }
  cout << ")";
}

void printIntRange(IntRange ir)
{
  cout << "(" << ir._low << "-" << ir._high << ")";
}

void printStringList(vector<string> vs)
{
  bool first = true;
  cout << "(";
  for (vector<string>::iterator i = vs.begin();
       i != vs.end(); ++i)
  {
    if (! first) cout << ",";
    cout << "\"" << *i << "\"";
    first = false;
  }
  cout << ")";
}

int main(int argc, char *argv[])
{
  try
  {
    {
      cout << "Test 1" << endl;
      Parser p((string)",(\"ME\", \"SM\"," +
               "\"Which of the three items does not belong here?\")");

      vector<string> vs1 = p.parseStringList(true);
      p.parseComma();
      vector<string> vs2 = p.parseStringList();
      bool comma = p.parseComma(true);

      printStringList(vs1);
      cout << ",";
      printStringList(vs2);
      if (comma) cout << ",";
      cout << endl << endl;
    }
    {
      cout << "Test 2" << endl;
      Parser p("(1-5),425,\"+abcd\"efgh\"");

      IntRange ir = p.parseRange();
      p.parseComma();
      int i = p.parseInt();
      p.parseComma();
      string s = p.parseString(false, true);

      printIntRange(ir);
      cout << "," << i << ",\"" << s << "\"" << endl << endl;
    }
    {
      cout << "Test 3" << endl;
      Parser p("(7,1-5,12-11,25),425,This is a test.\"+a\"ef\" and so forth");

      vector<bool> vb = p.parseIntList();
      p.parseComma();
      int i = p.parseInt();
      p.parseComma();
      string s = p.parseEol();

      printIntList(vb);
      cout << "," << i << "," << s << endl << endl;
    }
    {
      cout << "Test 4" << endl;
      Parser p("(1-125),20,16");

      vector<bool> vb = p.parseIntList();
      p.parseComma();
      vector<bool> vb2 = p.parseIntList();
      p.parseComma();
      int j = p.parseInt();

      printIntList(vb);
      cout << ",";
      printIntList(vb2);
      cout << "," << j << endl << endl;
    }
    {
      cout << "Test 5" << endl;
      Parser p("SM,7");

      string s = p.parseString();
      p.parseComma();
      int i = p.parseInt();

      cout << s << "," << i << endl << endl;
    }
    {
      cout << "Test 6" << endl;
      Parser p("(2,\"S TELIA MOBITEL\",\"S TELIA\",\"24001\")");

      p.parseChar('(');
      int status = p.parseInt();
      p.parseComma();
      string longName = p.parseString(true);
      p.parseComma();
      string shortName = p.parseString(true);
      p.parseComma();
      int numericName;
      try
      {
        numericName = p.parseInt(true);
      }
      catch (GsmException &e)
      {
        if (e.getErrorClass() == ParserError)
        {
          // the Ericsson GM12 GSM modem returns the numeric ID as string
          string s = p.parseString();
          numericName = checkNumber(s);
        }
        else
          throw e;
      }
      p.parseChar(')');
      
      cout << "(" << status << ",\"" << longName << "\",\""
           << shortName << "\","
           << numericName << ")" << endl << endl;
    }
  }
  catch (GsmException &p)
  {
    // these tests shouldn't throw exceptions
    assert(0);
  }

  // Now some tests that should provoke an error
  try
  {
    Parser p("(4-5");
    p.parseRange();
  }
  catch (GsmException &p)
  {
    cout << "Error 1: " << p.what() << endl << endl;
  }
  try
  {
    Parser p("(4-5,3-4-5)");
    p.parseIntList();
  }
  catch (GsmException &p)
  {
    cout << "Error 2: " << p.what() << endl << endl;
  }
  try
  {
    Parser p("\"bla\"bla\"");
    p.parseString();
    p.checkEol();
  }
  catch (GsmException &p)
  {
    cout << "Error 3: " << p.what() << endl << endl;
  }

}
