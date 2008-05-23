#ifndef XMLRPCMETHOD_HPP
#define XMLRPCMETHOD_HPP

#include <string>
#include <iostream>
#include <xmlrpc-c/base.hpp>

class xmlrpcMethod {
    // An object of this class contains everything we know about a
    // given XML-RPC method, and knows how to print local bindings.

    std::string mFunctionName;
    std::string mMethodName;
    std::string mHelp;
    xmlrpc_c::value_array mSynopsis;

public: 
    xmlrpcMethod(std::string            const& function_name,
                 std::string           const& method_name,
                 std::string           const& help,
                 xmlrpc_c::value_array const& signatureList);

    xmlrpcMethod(xmlrpcMethod const& f);

    xmlrpcMethod& operator= (xmlrpcMethod const& f);
    
    void
    printDeclarations(std::ostream& out) const;

    void
    printDefinitions(std::ostream     & out,
                     std::string const& className) const;

private:
    void
    printParameters(std::ostream      & out,
                    size_t       const  synopsis_index) const;

    void
    printDeclaration(std::ostream       & out,
                     size_t       const   synopsis_index) const;

    void
    printDefinition(std::ostream     & out,
                    std::string const& className,
                    size_t      const  synopsis_index) const;

    const xmlrpcType&
    returnType(size_t const synopsis_index) const;

    size_t
    parameterCount(size_t const synopsis_index) const;

    const xmlrpcType&
    parameterType(size_t const synopsis_index,
                  size_t const parameter_index) const;
};

#endif
