#include <gsmlib/gsm_unix_serial.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <iostream>
#include <strstream>

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
    cout << (string)"Opening device " + argv[1] << endl;
    Ref<Port> port = new UnixSerialPort((string)argv[1], B38400);

    cout << "Creating MeTa object" << endl;
    MeTa m(port);

    cout << "Getting phonebook entries" << endl;
    vector<string> pbs = m.getPhoneBookStrings();
    for (vector<string>::iterator i = pbs.begin(); i != pbs.end(); ++i)
    {
      PhonebookRef pb = m.getPhonebook(*i);

      cout << "Phonebook \"" << *i << "\" " << endl
           << "  Max number length: " << pb->getMaxTelephoneLen() << endl
           << "  Max text length: " << pb->getMaxTextLen() << endl
           << "  Capacity: " << pb->capacity() << endl
           << "  Size: " << pb->size() << endl;

      for (Phonebook::iterator j = pb->begin(); j != pb->end(); ++j)
        if (! j->empty())
          cout << "  Entry #" << j - pb->begin()
               << "Number: \"" << j->telephone() << "\""
               << "Text: \"" << j->text() << "\"" << endl;
    }
  }
  catch (GsmException &ge)
  {
    cerr << "GsmException '" << ge.what() << "'" << endl;
    return 1;
  }
  return 0;
}
