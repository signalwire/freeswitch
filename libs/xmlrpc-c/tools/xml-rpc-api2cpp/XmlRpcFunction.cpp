#include <iostream>
#include <stdexcept>

#include "xmlrpc-c/oldcppwrapper.hpp"
#include "DataType.hpp"
#include "XmlRpcFunction.hpp"

using std::domain_error;
using std::endl;


//=========================================================================
//  class XmlRpcFunction
//=========================================================================
//  Contains everything we know about a given server function, and knows
//  how to print local bindings.

XmlRpcFunction::XmlRpcFunction(const string& function_name,
			       const string& method_name,
			       const string& help,
			       XmlRpcValue synopsis)
    : mFunctionName(function_name), mMethodName(method_name),
      mHelp(help), mSynopsis(synopsis)
{
}

XmlRpcFunction::XmlRpcFunction (const XmlRpcFunction& f)
    : mFunctionName(f.mFunctionName), mMethodName(f.mMethodName),
      mHelp(f.mHelp), mSynopsis(f.mSynopsis)
{
}

XmlRpcFunction& XmlRpcFunction::operator= (const XmlRpcFunction& f) {
    if (this == &f)
	return *this;
    mFunctionName = f.mFunctionName;
    mMethodName = f.mMethodName;
    mHelp = f.mHelp;
    mSynopsis = f.mSynopsis;
    return *this;
}

void XmlRpcFunction::printDeclarations (ostream& out) {

    // XXX - Do a sloppy job of printing documentation.
    out << endl << "    /* " << mHelp << " */" << endl;

    // Print each declaration.
    size_t end = mSynopsis.arraySize();
    for (size_t i = 0; i < end; i++)
	printDeclaration(out, i);
}

void XmlRpcFunction::printDefinitions (ostream& out, const string& className) {
    size_t end = mSynopsis.arraySize();
    for (size_t i = 0; i < end; i++) {
	out << endl;
	printDefinition(out, className, i);
    }
}

// Print the parameter declarations.
void XmlRpcFunction::printParameters (ostream& out, size_t synopsis_index) {
    size_t end = parameterCount(synopsis_index);
    bool first = true;
    for (size_t i = 0; i < end; i++) {
	if (first)
	    first = false;
	else
	    out << ", ";

	const DataType& ptype (parameterType(synopsis_index, i));
	string basename = ptype.defaultParameterBaseName(i + 1);
	out << ptype.parameterFragment(basename);
    }
}

void XmlRpcFunction::printDeclaration (ostream& out, size_t synopsis_index) {
    const DataType& rtype (returnType(synopsis_index));
    out << "    " << rtype.returnTypeFragment() << " "
	<< mFunctionName << " (";
    printParameters(out, synopsis_index);
    out << ");" << endl;
}

void XmlRpcFunction::printDefinition (ostream& out,
				      const string& className,
				      size_t synopsis_index)
{
    const DataType& rtype (returnType(synopsis_index));
    out << rtype.returnTypeFragment() << " "
	<< className << "::" << mFunctionName << " (";
    printParameters(out, synopsis_index);
    out << ") {" << endl;    
    out << "    XmlRpcValue params = XmlRpcValue::makeArray();" << endl;

    /* Emit code to convert the parameters into an array of XML-RPC objects. */
    size_t end = parameterCount(synopsis_index);
    for (size_t i = 0; i < end; i++) {
	const DataType& ptype (parameterType(synopsis_index, i));
	string basename = ptype.defaultParameterBaseName(i + 1);
	out << "    params.arrayAppendItem("
	    << ptype.inputConversionFragment(basename) << ");" << endl;
    }

    /* Emit the function call.*/
    out << "    XmlRpcValue result = this->mClient.call(\""
	<< mMethodName << "\", params);" << endl;    

    /* Emit the return statement. */
    out << "    return " << rtype.outputConversionFragment("result")
	<< ";" << endl;
    out << "}" << endl;
}

const DataType& XmlRpcFunction::returnType (size_t synopsis_index) {
    XmlRpcValue func_synop = mSynopsis.arrayGetItem(synopsis_index);
    return findDataType(func_synop.arrayGetItem(0).getString());
}

size_t XmlRpcFunction::parameterCount (size_t synopsis_index) {
    XmlRpcValue func_synop = mSynopsis.arrayGetItem(synopsis_index);
    size_t size = func_synop.arraySize();
    if (size < 1)
	throw domain_error("Synopsis contained no items");
    return size - 1;
}

const DataType& XmlRpcFunction::parameterType (size_t synopsis_index,
					       size_t parameter_index)
{
    XmlRpcValue func_synop = mSynopsis.arrayGetItem(synopsis_index);
    XmlRpcValue param = func_synop.arrayGetItem(parameter_index + 1);
    return findDataType(param.getString());
}


