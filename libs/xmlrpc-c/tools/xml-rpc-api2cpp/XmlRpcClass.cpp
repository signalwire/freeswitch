#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

#include "xmlrpc-c/oldcppwrapper.hpp"

#include "DataType.hpp"
#include "XmlRpcFunction.hpp"
#include "XmlRpcClass.hpp"


XmlRpcClass::XmlRpcClass(string const& className) :
    mClassName(className) {}



XmlRpcClass::XmlRpcClass(XmlRpcClass const& c) :
    mClassName(c.mClassName),
    mFunctions(c.mFunctions) {}



XmlRpcClass&
XmlRpcClass::operator= (XmlRpcClass const& c) {

    if (this != &c) {
        this->mClassName = c.mClassName;
        this->mFunctions = c.mFunctions;
    }
    return *this;
}



void
XmlRpcClass::addFunction(XmlRpcFunction const& function) {

    mFunctions.push_back(function);
}



void
XmlRpcClass::printDeclaration(ostream & out) const {

    out << "class " << mClassName << " {" << endl;
    out << "    XmlRpcClient mClient;" << endl;
    out << endl;
    out << "public:" << endl;
    out << "    " << mClassName << " (const XmlRpcClient& client)" << endl;
    out << "        : mClient(client) {}" << endl;
    out << "    " << mClassName << " (const std::string& server_url)" << endl;
    out << "        : mClient(XmlRpcClient(server_url)) {}" << endl;
    out << "    " << mClassName << " (const " << mClassName << "& o)" << endl;
    out << "        : mClient(o.mClient) {}" << endl;
    out << endl;
    out << "    " << mClassName << "& operator= (const "
         << mClassName << "& o) {" << endl;
    out << "        if (this != &o) mClient = o.mClient;" << endl;
    out << "        return *this;" << endl;
    out << "    }" << endl;

    vector<XmlRpcFunction>::const_iterator f;
    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
        f->printDeclarations(out);
    }

    out << "};" << endl;    
}



void
XmlRpcClass::printDefinition(ostream & out) const {

    vector<XmlRpcFunction>::const_iterator f;

    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
        f->printDefinitions(out, mClassName);
    }
}

