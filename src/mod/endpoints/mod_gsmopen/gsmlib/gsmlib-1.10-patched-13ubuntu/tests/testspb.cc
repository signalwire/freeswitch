#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_sorted_phonebook.h>
#include <algorithm>
#include <strstream>
#include <iostream>

using namespace std;
using namespace gsmlib;

void printPb(PhonebookEntry &e)
{
  cout << "number: " << e.telephone()
       << " text: " << e.text() << endl;
}

int main(int argc, char *argv[])
{
  try
  {
    // open phonebook file
    SortedPhonebook pb((string)"spb-copy.pb", false);
    
    // print all entries
    cout << "Entries in pbs-copy.pb:" << endl;
    for (SortedPhonebook::iterator i = pb.begin(); i != pb.end(); ++i)
      cout << "  Text: " << i->text()
           << "  Telephone: " << i->telephone() << endl;

    // remove all entries with telephone == "0815"
    cout << "Removing entries with telephone == 0815" << endl;
    pb.setSortOrder(ByTelephone);

    string s = "0815";
    pb.erase(s);

    cout << "Entries in pbs-copy.pb<2>:" << endl;
    for (SortedPhonebook::iterator i = pb.begin(); i != pb.end(); ++i)
      cout << "  Text: " << i->text()
           << "  Telephone: " << i->telephone() << endl;

    // insert some entries
    cout << "Inserting some entries" << endl;
    pb.insert(PhonebookEntryBase("08152", "new line with \r continued"));
    pb.insert(PhonebookEntryBase("41598254", "Hans-Dieter Schmidt"));
    pb.insert(PhonebookEntryBase("34058", "Hans-Dieter|Hofmann"));

    pb.setSortOrder(ByText);
    cout << "Entries in pbs-copy.pb<3>:" << endl;
    for (SortedPhonebook::iterator i = pb.begin(); i != pb.end(); ++i)
      cout << "  Text: " << i->text()
           << "  Telephone: " << i->telephone() << endl;

    // test erasing all "Hans-Dieter Schmidt" entries
    cout << "Erasing all Hans-Dieter Schmidt entries" << endl;
    s = "Hans-Dieter Schmidt";
    pair<SortedPhonebook::iterator, SortedPhonebook::iterator> range =
      pb.equal_range(s);
    cout << "About to erase:" << endl;
    for (SortedPhonebook::iterator i = range.first; i != range.second; ++i)
      cout << "  Text: " << i->text()
           << "  Telephone: " << i->telephone() << endl;
    
    pb.erase(range.first, range.second);

    // write back to file
    cout << "Writing back to file" << endl;
    pb.sync();

    // tests the NoCopy class
    //SortedPhonebook pb2("spb.pb");
    //pb2 = pb;
  }
  catch (GsmException &ge)
  {
    cerr << "GsmException '" << ge.what() << "'" << endl;
    return 1;
  }
  return 0;
}
