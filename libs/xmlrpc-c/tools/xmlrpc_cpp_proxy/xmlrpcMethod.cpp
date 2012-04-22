#include <iostream>
#include <sstream>
#include <stdexcept>

#include "xmlrpcType.hpp"

#include "xmlrpcMethod.hpp"

using namespace std;


xmlrpcMethod::xmlrpcMethod(string                const& functionName,
                           string                const& methodName,
                           string                const& help,
                           xmlrpc_c::value_array const& signatureList) :
    mFunctionName(functionName),
    mMethodName(methodName),
    mHelp(help),
    mSynopsis(signatureList) {}



xmlrpcMethod::xmlrpcMethod(xmlrpcMethod const& f) :
    mFunctionName(f.mFunctionName),
    mMethodName(f.mMethodName),
    mHelp(f.mHelp),
    mSynopsis(f.mSynopsis) {}



xmlrpcMethod&
xmlrpcMethod::operator= (xmlrpcMethod const& f) {

    if (this != &f) {
        this->mFunctionName = f.mFunctionName;
        this->mMethodName   = f.mMethodName;
        this->mHelp         = f.mHelp;
        this->mSynopsis     = f.mSynopsis;
    }
    return *this;
}






size_t
xmlrpcMethod::parameterCount(size_t const synopsisIndex) const {

    xmlrpc_c::value_array const funcSynop(
        mSynopsis.vectorValueValue()[synopsisIndex]);
    size_t const size(funcSynop.size());

    if (size < 1)
        throw domain_error("Synopsis contains no items");

    return size - 1;
}



xmlrpcType const&
xmlrpcMethod::parameterType(size_t const synopsisIndex,
                            size_t const parameterIndex) const {

    xmlrpc_c::value_array const funcSynop(
        mSynopsis.vectorValueValue()[synopsisIndex]);
    xmlrpc_c::value_string const param(
        funcSynop.vectorValueValue()[parameterIndex + 1]);

    return findXmlrpcType(static_cast<string>(param));
}



const xmlrpcType&
xmlrpcMethod::returnType(size_t const synopsisIndex) const {

    xmlrpc_c::value_array const funcSynop(
        mSynopsis.vectorValueValue()[synopsisIndex]);

    xmlrpc_c::value_string datatype(funcSynop.vectorValueValue()[0]);
    return findXmlrpcType(static_cast<string>(datatype));
}



void
xmlrpcMethod::printParameters(ostream      & out,
                              size_t  const  synopsisIndex) const {
/*----------------------------------------------------------------------------
  Print the parameter declarations.
-----------------------------------------------------------------------------*/
    size_t const end(parameterCount(synopsisIndex));

    bool first;

    first = true;

    for (size_t i = 0; i < end; ++i) {
        if (!first)
            out << ", ";

        xmlrpcType const& ptype(parameterType(synopsisIndex, i));

        string const localName(ptype.defaultParameterBaseName(i + 1));
        out << ptype.parameterFragment(localName);

        first = false;
    }
}



void
xmlrpcMethod::printDeclaration(ostream      & out,
                               size_t  const  synopsisIndex) const {

    try {
        xmlrpcType const& rtype(returnType(synopsisIndex));

        out << "    " << rtype.returnTypeFragment() << " "
            << mFunctionName << " (";

        printParameters(out, synopsisIndex);
        
        out << ");" << endl;
    } catch (xmlrpc_c::fault const& f) {
        ostringstream msg;

        msg << "Failed to generate header for signature "
            << synopsisIndex
            << " .  "
            << f.getDescription();

        throw(logic_error(msg.str()));
    }
}



void
xmlrpcMethod::printDeclarations(ostream & out) const {

    try {
        // Print the method help as a comment

        out << endl << "    /* " << mHelp << " */" << endl;

        size_t end;

        try {
            end = mSynopsis.size();
        } catch (xmlrpc_c::fault const& f) {
            throw(logic_error("Failed to get size of signature array for "
                              "method " + this->mFunctionName + ".  " +
                              f.getDescription()));
        }
        // Print the declarations for all the signatures of this
        // XML-RPC method.
                                      
        for (size_t i = 0; i < end; ++i)
            printDeclaration(out, i);
        
    } catch (exception const& e) {
        throw(logic_error("Failed to generate declarations for method " +
                          this->mFunctionName + ".  " + e.what()));
    }
}



void
xmlrpcMethod::printDefinition(ostream     & out,
                              string const& className,
                              size_t const  synopsisIndex) const {

    xmlrpcType const& rtype(returnType(synopsisIndex));

    out << rtype.returnTypeFragment() << " "
        << className << "::" << mFunctionName << " (";

    printParameters(out, synopsisIndex);

    out << ") {" << endl;

    size_t const end(parameterCount(synopsisIndex));
    if (end > 0){
        // Emit code to generate the parameter list object
        out << "    xmlrpc_c::paramList params;" << endl;
        for (size_t i = 0; i < end; ++i) {
            xmlrpcType const& ptype(parameterType(synopsisIndex, i));
            string const basename(ptype.defaultParameterBaseName(i + 1));
            out << "    params.add("
                << ptype.inputConversionFragment(basename) << ");" << endl;
        }
    }

    // Emit result holder declaration.
    out << "    xmlrpc_c::value result;" << endl;

    // Emit the call to the XML-RPC call method
    out << "    this->client.call("
        << "this->serverUrl, "
        << "\"" << mMethodName << "\", ";

    if (end > 0)
        out << "params, ";

    out << "&result);" << endl;    

    // Emit the return statement.
    out << "    return " << rtype.outputConversionFragment("result")
        << ";" << endl;
    out << "}" << endl;
}



void
xmlrpcMethod::printDefinitions(ostream      & out,
                               string  const& className) const {

    try {
        size_t const end(mSynopsis.size());
    
        for (size_t i = 0; i < end; ++i) {
            out << endl;
            printDefinition(out, className, i);
        }
    } catch (xmlrpc_c::fault const& f) {
        throw(logic_error("Failed to generate definitions for class " +
                          this->mFunctionName + ".  " +
                          f.getDescription()));
    }
}



