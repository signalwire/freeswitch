#include <vector>

class XmlRpcClass {
    std::string mClassName;
    std::vector<XmlRpcFunction> mFunctions;


public:
    XmlRpcClass (std::string class_name);
    XmlRpcClass (const XmlRpcClass&);
    XmlRpcClass& operator= (const XmlRpcClass&);

    std::string className () const { return mClassName; }

    void addFunction (const XmlRpcFunction& function);

    void printDeclaration (ostream& out);
    void printDefinition (ostream& out);
};
