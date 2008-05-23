#include <string>
#include <cassert>

class xmlrpcType {
    std::string mTypeName;

    xmlrpcType(xmlrpcType const&) { assert(false); }
    
    xmlrpcType& operator= (xmlrpcType const&) {
        assert(false);
        return *this;
    }

public:
    xmlrpcType(const std::string& type_name) : mTypeName(type_name) {}

    virtual ~xmlrpcType () {}

    // Return the name for this XML-RPC type.
    virtual std::string
    typeName() const { return mTypeName; }

    // Given a parameter position, calculate a unique base name for all
    // parameter-related variables.
    virtual std::string
    defaultParameterBaseName(unsigned int const position) const;

    // Virtual functions for processing parameters.
    virtual std::string
    parameterFragment(std::string const& base_name) const = 0;

    virtual std::string
    inputConversionFragment(std::string const& base_name) const = 0;

    // Virtual functions for processing return values.
    virtual std::string
    returnTypeFragment () const = 0;

    virtual std::string
    outputConversionFragment(std::string const& var_name) const = 0;
};

const xmlrpcType& findXmlrpcType(const std::string& name);
