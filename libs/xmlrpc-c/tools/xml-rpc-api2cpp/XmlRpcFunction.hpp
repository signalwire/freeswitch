#include <string>
#include <iostream>

using std::string;
using std::ostream;

class XmlRpcFunction {
    // An object of this class contains everything we know about a
    // given XML-RPC method, and knows how to print local bindings.

    string mFunctionName;
    string mMethodName;
    string mHelp;
    XmlRpcValue mSynopsis;

public: 
    XmlRpcFunction(const string& function_name,
                   const string& method_name,
                   const string& help,
                   XmlRpcValue synopsis);

    XmlRpcFunction (const XmlRpcFunction&);
    XmlRpcFunction& operator= (const XmlRpcFunction&);
    
    void printDeclarations (ostream& out) const;
    void printDefinitions  (ostream& out, const string& className) const;

private:
    void printParameters  (ostream& out, size_t synopsis_index) const;
    void printDeclaration (ostream& out, size_t synopsis_index) const;
    void printDefinition  (ostream& out,
                           const string& className,
                           size_t synopsis_index) const;

    const DataType& returnType (size_t synopsis_index) const;
    size_t parameterCount (size_t synopsis_index) const;
    const DataType& parameterType (size_t synopsis_index,
                                   size_t parameter_index) const;
};
