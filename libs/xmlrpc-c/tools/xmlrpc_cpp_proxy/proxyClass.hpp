#include <vector>
#include <string>

#include "xmlrpcMethod.hpp"

class proxyClass {
    //  An object of this class contains information about a proxy
    //  class, and knows how to generate code.

public:
    proxyClass(std::string const& className);

    proxyClass(proxyClass const&);

    std::string
    className() const;

    void
    addFunction(xmlrpcMethod const& function);

    void
    printDeclaration(std::ostream& out) const;

    void
    printDefinition(std::ostream& out) const;

private:

    std::string const _className;

    std::vector<xmlrpcMethod> functions;
};
