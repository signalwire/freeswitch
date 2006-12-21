#include <iostream>
#include <sstream>
#include <stdexcept>

#include "xmlrpc-c/oldcppwrapper.hpp"
#include "DataType.hpp"

using namespace std;



//=========================================================================
//  abstract class DataType
//=========================================================================
//  Instances of DataType know how generate code fragments for manipulating
//  a specific XML-RPC data type.

string DataType::defaultParameterBaseName (int position) const {
    ostringstream name_stream;
    name_stream << typeName() << position << ends;
    string name(name_stream.str());
    return name;
}


//=========================================================================
//  class RawDataType
//=========================================================================
//  We want to manipulate some XML-RPC data types as XmlRpcValue objects.

class RawDataType : public DataType {
public:
    RawDataType (const string& type_name) : DataType(type_name) {}
    
    virtual string parameterFragment (const string& base_name) const;
    virtual string inputConversionFragment (const string& base_name) const;
    virtual string returnTypeFragment () const;
    virtual string outputConversionFragment (const string& var_name) const;
};

string RawDataType::parameterFragment (const string& base_name) const {
    return "XmlRpcValue /*" + typeName() + "*/ " + base_name;
}

string RawDataType::inputConversionFragment (const string& base_name) const {
    return base_name;
}

string RawDataType::returnTypeFragment () const {
    return "XmlRpcValue /*" + typeName() + "*/";
}

string RawDataType::outputConversionFragment (const string& var_name) const {
    return var_name;
}


//=========================================================================
//  class SimpleDataType
//=========================================================================
//  Other types can be easily converted to and from a single native type.

class SimpleDataType : public DataType {
    string mNativeType;
    string mMakerFunc;
    string mGetterFunc;

public:
    SimpleDataType (const string& type_name,
		    const string& native_type,
		    const string& maker_func,
		    const string& getter_func);

    virtual string parameterFragment (const string& base_name) const;
    virtual string inputConversionFragment (const string& base_name) const;
    virtual string returnTypeFragment () const;
    virtual string outputConversionFragment (const string& var_name) const;
};

SimpleDataType::SimpleDataType (const string& type_name,
				const string& native_type,
				const string& maker_func,
				const string& getter_func)
    : DataType(type_name),
      mNativeType(native_type),
      mMakerFunc(maker_func),
      mGetterFunc(getter_func)
{
}

string SimpleDataType::parameterFragment (const string& base_name) const {
    return mNativeType + " " + base_name;
}

string SimpleDataType::inputConversionFragment (const string& base_name) const
{
    return mMakerFunc + "(" + base_name + ")";
}

string SimpleDataType::returnTypeFragment () const {
    return mNativeType; 
}

string SimpleDataType::outputConversionFragment (const string& var_name) const
{
    return var_name + "." + mGetterFunc + "()";
}


//=========================================================================
//  class VoidDataType
//=========================================================================
//  Some XML-RPC servers declare functions as void.  Such functions have
//  an arbitrary return value which we should ignore.

class VoidDataType : public DataType {
public:
    VoidDataType () : DataType("void") {}
    
    virtual string parameterFragment (const string& base_name) const;
    virtual string inputConversionFragment (const string& base_name) const;
    virtual string returnTypeFragment () const;
    virtual string outputConversionFragment (const string& var_name) const;
};

string VoidDataType::parameterFragment (const string&) const {
    throw domain_error("Can't handle functions with 'void' arguments'");
    
}

string VoidDataType::inputConversionFragment (const string&) const {
    throw domain_error("Can't handle functions with 'void' arguments'");
}

string VoidDataType::returnTypeFragment () const {
    return "void";
}

string VoidDataType::outputConversionFragment (const string&) const {
    return "/* Return value ignored. */";
}


//=========================================================================
//  function findDataType
//=========================================================================
//  Given the name of an XML-RPC data type, try to find a corresponding
//  DataType object.

SimpleDataType intType    ("int", "XmlRpcValue::int32",
			   "XmlRpcValue::makeInt",
			   "getInt");
SimpleDataType boolType   ("bool", "bool",
			   "XmlRpcValue::makeBool",
			   "getBool");
SimpleDataType doubleType ("double", "double",
			   "XmlRpcValue::makeDouble",
			   "getDouble");
SimpleDataType stringType ("string", "string",
			   "XmlRpcValue::makeString",
			   "getString");

RawDataType dateTimeType  ("dateTime");
RawDataType base64Type    ("base64");
RawDataType structType    ("struct");
RawDataType arrayType     ("array");

VoidDataType voidType;

const DataType& findDataType (const string& name) {
    if (name == "int" || name == "i4")
	return intType;
    else if (name == "boolean")
	return boolType;
    else if (name == "double")
	return doubleType;
    else if (name == "string")
	return stringType;
    else if (name == "dateTime.iso8601")
	return dateTimeType;
    else if (name == "base64")
	return base64Type;
    else if (name == "struct")
	return structType;
    else if (name == "array")
	return arrayType;
    else if (name == "void")
	return voidType;
    else
	throw domain_error("Unknown XML-RPC type " + name);
    
    // This code should never be executed.
    XMLRPC_ASSERT(0);
    return intType;
}
