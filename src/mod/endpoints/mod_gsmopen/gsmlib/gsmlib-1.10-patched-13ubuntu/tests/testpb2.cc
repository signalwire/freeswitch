#include <gsmlib/gsm_unix_serial.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <strstream>
#include <iostream>

using namespace std;
using namespace gsmlib;

bool isbla(PhonebookEntry &e)
{
  //  cerr << "****'" << e.text() << "'" << endl;
  return e.text() == "blabla";
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
    if (pbs.begin() == pbs.end())
    {
      cerr << "no phonebooks available" << endl;
      exit(1);
    }
    
    PhonebookRef pb = m.getPhonebook(*pbs.begin());

    cout << "Phonebook \"" << pb->name() << "\" " << endl
         << "  Max number length: " << pb->getMaxTelephoneLen() << endl
         << "  Max text length: " << pb->getMaxTextLen() << endl
         << "  Capacity: " << pb->capacity() << endl;

    cout << "Inserting entry 'blabla'" << endl;
    PhonebookEntry e("123456", "blabla");
    pb->insert(pb->end(), e);

    int j = -1;
    for (int i = 50; i < 60; ++i)
      if (pb()[i].empty())
      {
        pb()[i].set("23456", "blabla");
        j = i;
        break;
      }
    
    pb->erase(pb->begin() + j);

    Phonebook::iterator k;
    do
    {
      k = find_if(pb->begin(), pb->end(), isbla);
      if (k != pb->end())
      {
        cerr << "Erasing #" << k - pb->begin() << endl;
        pb->erase(k, k + 1);
      }
    }
    while (k != pb->end());
  }
  catch (GsmException &ge)
  {
    cerr << "GsmException '" << ge.what() << "'" << endl;
    return 1;
  }
  return 0;
}
