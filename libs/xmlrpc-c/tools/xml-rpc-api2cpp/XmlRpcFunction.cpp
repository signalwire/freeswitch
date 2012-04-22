#include <iostream>
#include <sstream>
#include <stdexcept>

#include "xmlrpc-c/oldcppwrapper.hpp"
#include "DataType.hpp"
#include "XmlRpcFunction.hpp"

using namespace std;


XmlRpcFunction::XmlRpcFunction(string      const& functionName,
                               string      const& methodName,
                               string      const& help,
                               XmlRpcValue const  signatureList) :
    mFunctionName(functionName),
    mMethodName(methodName),
    mHelp(help),
    mSynopsis(signatureList) {}



XmlRpcFunction::XmlRpcFunction(XmlRpcFunction const& f) :
    mFunctionName(f.mFunctionName),
    mMethodName(f.mMethodName),
    mHelp(f.mHelp),
    mSynopsis(f.mSynopsis) {}



XmlRpcFunction&
XmlRpcFunction::operator= (XmlRpcFunction const& f) {

    if (this != &f) {
        this->mFunctionName = f.mFunctionName;
        this->mMethodName   = f.mMethodName;
        this->mHelp         = f.mHelp;
        this->mSynopsis     = f.mSynopsis;
    }
    return *this;
}



void
XmlRpcFunction::printDeclarations(ostream & out) const {

    try {
        // Print the method help as a comment

        out << endl << "    /* " << mHelp << " */" << endl;

        size_t end;

        try {
            end = mSynopsis.arraySize();
        } catch (XmlRpcFault const& fault) {
            throw(logic_error("Failed to get size of signature array for "
                              "method " + this->mFunctionName + ".  " +
                              fault.getFaultString()));
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
XmlRpcFunction::printDefinitions(ostream      & out,
                                 string  const& className) const {

    try {
        size_t const end(mSynopsis.arraySize());
    
        for (size_t i = 0; i < end; ++i) {
            out << endl;
            printDefinition(out, className, i);
        }
    } catch (XmlRpcFault const& fault) {
        throw(logic_error("Failed to generate definitions for class " +
                          this->mFunctionName + ".  " +
                          fault.getFaultString()));
    }
}



void
XmlRpcFunction::printParameters(ostream      & out,
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

        DataType const& ptype(parameterType(synopsisIndex, i));
        string const basename(ptype.defaultParameterBaseName(i + 1));
        out << ptype.parameterFragment(basename);

        first = false;
    }
}



void
XmlRpcFunction::printDeclaration(ostream      & out,
                                 size_t  const  synopsisIndex) const {

    try {
        DataType const& rtype(returnType(synopsisIndex));

        out << "    " << rtype.returnTypeFragment() << " "
            << mFunctionName << " (";

        printParameters(out, synopsisIndex);
        
        out << ");" << endl;
    } catch (XmlRpcFault const& fault) {
        ostringstream msg;

        msg << "Failed to generate header for signature "
            << synopsisIndex
            << " .  "
            << fault.getFaultString();

        throw(logic_error(msg.str()));
    }
}



void
XmlRpcFunction::printDefinition(ostream     & out,
                                string const& className,
                                size_t const  synopsisIndex) const {

    DataType const& rtype(returnType(synopsisIndex));

    out << rtype.returnTypeFragment() << " "
        << className << "::" << mFunctionName << " (";

    printParameters(out, synopsisIndex);

    out << ") {" << endl;    
    out << "    XmlRpcValue params(XmlRpcValue::makeArray());" << endl;

    /* Emit code to convert the parameters into an array of XML-RPC objects. */
    size_t const end(parameterCount(synopsisIndex));
    for (size_t i = 0; i < end; ++i) {
        DataType const& ptype(parameterType(synopsisIndex, i));
        string const basename(ptype.defaultParameterBaseName(i + 1));
        out << "    params.arrayAppendItem("
            << ptype.inputConversionFragment(basename) << ");" << endl;
    }

    /* Emit the function call.*/
    out << "    XmlRpcValue result(this->mClient.call(\""
        << mMethodName << "\", params));" << endl;    

    /* Emit the return statement. */
    out << "    return " << rtype.outputConversionFragment("result")
        << ";" << endl;
    out << "}" << endl;
}



const DataType&
XmlRpcFunction::returnType(size_t const synopsisIndex) const {

    XmlRpcValue const funcSynop(mSynopsis.arrayGetItem(synopsisIndex));

    return findDataType(funcSynop.arrayGetItem(0).getString());
}



size_t
XmlRpcFunction::parameterCount(size_t const synopsisIndex) const {

    XmlRpcValue const funcSynop(mSynopsis.arrayGetItem(synopsisIndex));
    size_t const size(funcSynop.arraySize());

    if (size < 1)
        throw domain_error("Synopsis contained no items");
    return size - 1;
}



DataType const&
XmlRpcFunction::parameterType(size_t const synopsisIndex,
                              size_t const parameterIndex) const {

    XmlRpcValue const funcSynop(mSynopsis.arrayGetItem(synopsisIndex));
    XmlRpcValue const param(funcSynop.arrayGetItem(parameterIndex + 1));

    return findDataType(param.getString());
}
