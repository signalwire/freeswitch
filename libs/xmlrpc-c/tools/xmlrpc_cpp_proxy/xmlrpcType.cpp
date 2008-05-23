#include <iostream>
#include <sstream>
#include <stdexcept>
#include <map>
#include <vector>

#include <xmlrpc-c/base.hpp>

#include "xmlrpcType.hpp"

using namespace std;



//=========================================================================
//  abstract class xmlrpcType
//=========================================================================
//  Instances of xmlrpcType know how generate code fragments for manipulating
//  a specific XML-RPC data type.

string
xmlrpcType::defaultParameterBaseName(unsigned int const position) const {

    ostringstream nameStream;

    nameStream << typeName() << position;

    return nameStream.str();
}



class rawXmlrpcType : public xmlrpcType {
public:
    rawXmlrpcType(string const& typeName) : xmlrpcType(typeName) {}
    
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
rawXmlrpcType::parameterFragment(string const& baseName) const {
    return "xmlrpc_c::value /*" + typeName() + "*/ " + baseName;
}



string
rawXmlrpcType::inputConversionFragment(string const& baseName) const {

    return baseName;
}



string
rawXmlrpcType::returnTypeFragment() const {
    return "xmlrpc_c::value /*" + typeName() + "*/";
}



string
rawXmlrpcType::outputConversionFragment(string const& varName) const {

    return varName;
}



class simpleXmlrpcType : public xmlrpcType {

    string mNativeType;
    string mMakerFunc;
    string mGetterFunc;

public:
    simpleXmlrpcType(string const& typeName,
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



simpleXmlrpcType::simpleXmlrpcType(string const& typeName,
                                   string const& nativeType,
                                   string const& makerFunc,
                                   string const& getterFunc)
    : xmlrpcType(typeName),
      mNativeType(nativeType),
      mMakerFunc(makerFunc),
      mGetterFunc(getterFunc) {
}



string
simpleXmlrpcType::parameterFragment(string const& baseName) const {

    return mNativeType + " const " + baseName;
}



string
simpleXmlrpcType::inputConversionFragment(string const& baseName) const {

    return mMakerFunc + "(" + baseName + ")";
}



string
simpleXmlrpcType::returnTypeFragment() const {

    return mNativeType; 
}



string
simpleXmlrpcType::outputConversionFragment(string const& varName) const {
    return mMakerFunc + "(" + varName + ")";
}



class voidXmlrpcType : public xmlrpcType {
public:
    voidXmlrpcType() : xmlrpcType("void") {}
    
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
voidXmlrpcType::parameterFragment(string const&) const {

    throw domain_error("Can't handle functions with 'void' arguments'");
}



string
voidXmlrpcType::inputConversionFragment(string const&) const {

    throw domain_error("Can't handle functions with 'void' arguments'");
}



string
voidXmlrpcType::returnTypeFragment () const {

    return "void";
}



string
voidXmlrpcType::outputConversionFragment(string const&) const {
    return "/* Return value ignored. */";
}



static simpleXmlrpcType const intType    ("int", "int",
                                          "xmlrpc_c::value_int",
                                          "getInt");
static simpleXmlrpcType const boolType   ("bool", "bool",
                                          "xmlrpc_c::value_boolean",
                                          "getBool");
static simpleXmlrpcType const doubleType ("double", "double",
                                          "xmlrpc_c::value_double",
                                          "getDouble");
static simpleXmlrpcType const stringType ("string", "std::string",
                                          "xmlrpc_c::value_string",
                                          "getString");

static rawXmlrpcType const dateTimeType  ("dateTime");
static rawXmlrpcType const base64Type    ("base64");
static rawXmlrpcType const structType    ("struct");
static rawXmlrpcType const arrayType     ("array");

static voidXmlrpcType const voidType;



const xmlrpcType&
findXmlrpcType(string const& name) {
/*----------------------------------------------------------------------------
  Given the name of an XML-RPC data type, try to find a corresponding
  xmlrpcType object.
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
