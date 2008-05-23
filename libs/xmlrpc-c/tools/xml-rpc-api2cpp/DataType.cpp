#include <iostream>
#include <sstream>
#include <stdexcept>
#include <map>
#include <vector>

#include "xmlrpc-c/oldcppwrapper.hpp"
#include "DataType.hpp"

using namespace std;



//=========================================================================
//  abstract class DataType
//=========================================================================
//  Instances of DataType know how generate code fragments for manipulating
//  a specific XML-RPC data type.

string
DataType::defaultParameterBaseName(unsigned int const position) const {

    ostringstream nameStream;

    nameStream << typeName() << position;

    return nameStream.str();
}



class RawDataType : public DataType {
public:
    RawDataType(string const& typeName) : DataType(typeName) {}
    
    virtual string
    parameterFragment(string const& baseName) const;

    virtual string
    inputConversionFragment(string const& baseName) const;

    virtual string
    returnTypeFragment() const;

    virtual string
    outputConversionFragment(string const& varName) const;
};



string
RawDataType::parameterFragment(string const& baseName) const {
    return "XmlRpcValue /*" + typeName() + "*/ " + baseName;
}



string
RawDataType::inputConversionFragment(string const& baseName) const {

    return baseName;
}



string
RawDataType::returnTypeFragment() const {
    return "XmlRpcValue /*" + typeName() + "*/";
}



string
RawDataType::outputConversionFragment(string const& varName) const {

    return varName;
}



class SimpleDataType : public DataType {
    string mNativeType;
    string mMakerFunc;
    string mGetterFunc;

public:
    SimpleDataType(string const& typeName,
                   string const& nativeType,
                   string const& makerFunc,
                   string const& getterFunc);

    virtual string
    parameterFragment(string const& baseName) const;

    virtual string
    inputConversionFragment(string const& baseName) const;

    virtual string
    returnTypeFragment() const;

    virtual string
    outputConversionFragment(string const& varName) const;
};



SimpleDataType::SimpleDataType(string const& typeName,
                               string const& nativeType,
                               string const& makerFunc,
                               string const& getterFunc)
    : DataType(typeName),
      mNativeType(nativeType),
      mMakerFunc(makerFunc),
      mGetterFunc(getterFunc) {
}



string
SimpleDataType::parameterFragment(string const& baseName) const {

    return mNativeType + " const " + baseName;
}



string
SimpleDataType::inputConversionFragment(string const& baseName) const {

    return mMakerFunc + "(" + baseName + ")";
}



string
SimpleDataType::returnTypeFragment() const {

    return mNativeType; 
}



string
SimpleDataType::outputConversionFragment(string const& varName) const {
    return varName + "." + mGetterFunc + "()";
}



class VoidDataType : public DataType {
public:
    VoidDataType() : DataType("void") {}
    
    virtual string
    parameterFragment(string const& baseName) const;

    virtual string
    inputConversionFragment(string const& baseName) const;

    virtual string
    returnTypeFragment() const;

    virtual string
    outputConversionFragment(string const& varName) const;
};



string
VoidDataType::parameterFragment(string const&) const {

    throw domain_error("Can't handle functions with 'void' arguments'");
}



string
VoidDataType::inputConversionFragment(string const&) const {

    throw domain_error("Can't handle functions with 'void' arguments'");
}



string
VoidDataType::returnTypeFragment () const {

    return "void";
}



string
VoidDataType::outputConversionFragment(string const&) const {
    return "/* Return value ignored. */";
}



static SimpleDataType const intType    ("int", "XmlRpcValue::int32",
                                        "XmlRpcValue::makeInt",
                                        "getInt");
static SimpleDataType const boolType   ("bool", "bool",
                                        "XmlRpcValue::makeBool",
                                        "getBool");
static SimpleDataType const doubleType ("double", "double",
                                        "XmlRpcValue::makeDouble",
                                        "getDouble");
static SimpleDataType const stringType ("string", "std::string",
                                        "XmlRpcValue::makeString",
                                        "getString");

static RawDataType const dateTimeType  ("dateTime");
static RawDataType const base64Type    ("base64");
static RawDataType const structType    ("struct");
static RawDataType const arrayType     ("array");

static VoidDataType const voidType;



const DataType&
findDataType(string const& name) {
/*----------------------------------------------------------------------------
  Given the name of an XML-RPC data type, try to find a corresponding
  DataType object.
-----------------------------------------------------------------------------*/
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
    else if (name == "INT")
        return intType;
    else if (name == "BOOLEAN")
        return boolType;
    else if (name == "DOUBLE")
        return doubleType;
    else if (name == "STRING")
        return stringType;
    else if (name == "DATETIME.ISO8601")
        return dateTimeType;
    else if (name == "BASE64")
        return base64Type;
    else if (name == "STRUCT")
        return structType;
    else if (name == "ARRAY")
        return arrayType;
    else if (name == "VOID")
        return voidType;
    else if (name == "NIL")
        return voidType;
    else
        throw domain_error("Unknown XML-RPC type name '" + name + "'");
}
