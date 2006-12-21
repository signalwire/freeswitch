
#include <string>
#include <iostream>

using std::string;
using std::ostream;

class XmlRpcFunction {
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
    
    void printDeclarations (ostream& out);
    void printDefinitions  (ostream& out, const string& className);

private:
    void printParameters  (ostream& out, size_t synopsis_index);
    void printDeclaration (ostream& out, size_t synopsis_index);
    void printDefinition  (ostream& out,
			   const string& className,
			   size_t synopsis_index);

    const DataType& returnType (size_t synopsis_index);
    size_t parameterCount (size_t synopsis_index);
    const DataType& parameterType (size_t synopsis_index,
				   size_t parameter_index);
};
