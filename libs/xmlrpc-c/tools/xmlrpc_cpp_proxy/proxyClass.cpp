#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

#include "xmlrpc-c/client_simple.hpp"

#include "xmlrpcType.hpp"
#include "xmlrpcMethod.hpp"

#include "proxyClass.hpp"


proxyClass::proxyClass(string const& className) :
    _className(className) {}



proxyClass::proxyClass(proxyClass const& c) :
    _className(c._className),
    functions(c.functions) {}



string
proxyClass::className() const {

    return this->_className;
}



void
proxyClass::addFunction(xmlrpcMethod const& function) {

    functions.push_back(function);
}



void
proxyClass::printDeclaration(ostream & out) const {

    out << "class " << this->_className << " {" << endl;
    out << endl;
    out << "public:" << endl;

    // emit the constructor prototype:
    out << "    " << this->_className << "(std::string const& serverUrl) "
        << endl
        << "        : serverUrl(serverUrl) {}"
        << endl;

    // emit the XML-RPC method method prototypes:
    vector<xmlrpcMethod>::const_iterator f;
    for (f = this->functions.begin(); f < this->functions.end(); ++f) {
        f->printDeclarations(out);
    }

    // emit the private data:

    out << "private:" << endl;

    out << "    xmlrpc_c::clientSimple client;" << endl;

    out << "    std::string const serverUrl;" << endl;
    out << "        // The URL for the server for which we are proxy" << endl;

    // emit the class closing:
    out << "};" << endl;    
}



void
proxyClass::printDefinition(ostream & out) const {

    vector<xmlrpcMethod>::const_iterator f;

    for (f = this->functions.begin(); f < this->functions.end(); ++f) {
        f->printDefinitions(out, this->_className);
    }
}

