#include <vector>

class XmlRpcClass {
    //  An object of this class contains information about a proxy
    //  class, and knows how to generate code.

    std::string mClassName;
    std::vector<XmlRpcFunction> mFunctions;


public:
    XmlRpcClass (std::string const& className);
    XmlRpcClass(XmlRpcClass const&);
    XmlRpcClass& operator= (XmlRpcClass const&);

    std::string className () const { return mClassName; }

    void addFunction (const XmlRpcFunction& function);

    void printDeclaration (ostream& out) const;
    void printDefinition (ostream& out) const;
};
