#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

#include "xmlrpc-c/oldcppwrapper.hpp"

#include "DataType.hpp"
#include "XmlRpcFunction.hpp"
#include "XmlRpcClass.hpp"


//=========================================================================
//  XmlRpcClass
//=========================================================================
//  This class stores information about a proxy class, and knows how to
//  generate code.

XmlRpcClass::XmlRpcClass (string class_name)
    : mClassName(class_name)
{
}

XmlRpcClass::XmlRpcClass (const XmlRpcClass& c)
    : mClassName(c.mClassName),
      mFunctions(c.mFunctions)
{
}

XmlRpcClass& XmlRpcClass::operator= (const XmlRpcClass& c)
{
    if (this == &c)
	return *this;
    mClassName = c.mClassName;
    mFunctions = c.mFunctions;
    return *this;
}

void XmlRpcClass::addFunction (const XmlRpcFunction& function)
{
    mFunctions.push_back(function);
}

void XmlRpcClass::printDeclaration (ostream&)
{
    cout << "class " << mClassName << " {" << endl;
    cout << "    XmlRpcClient mClient;" << endl;
    cout << endl;
    cout << "public:" << endl;
    cout << "    " << mClassName << " (const XmlRpcClient& client)" << endl;
    cout << "        : mClient(client) {}" << endl;
    cout << "    " << mClassName << " (const string& server_url)" << endl;
    cout << "        : mClient(XmlRpcClient(server_url)) {}" << endl;
    cout << "    " << mClassName << " (const " << mClassName << "& o)" << endl;
    cout << "        : mClient(o.mClient) {}" << endl;
    cout << endl;
    cout << "    " << mClassName << "& operator= (const "
	 << mClassName << "& o) {" << endl;
    cout << "        if (this != &o) mClient = o.mClient;" << endl;
    cout << "        return *this;" << endl;
    cout << "    }" << endl;

    vector<XmlRpcFunction>::iterator f;
    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
	f->printDeclarations(cout);
    }

    cout << "};" << endl;    
}

void XmlRpcClass::printDefinition (ostream&)
{
    vector<XmlRpcFunction>::iterator f;
    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
	f->printDefinitions(cout, mClassName);
    }
}
