#include <string>
#include <cassert>

class DataType {
    std::string mTypeName;

    DataType(DataType const&) { assert(false); }
    
    DataType& operator= (DataType const&) {
        assert(false);
        return *this;
    }

public:
    DataType(const std::string& type_name) : mTypeName(type_name) {}

    virtual ~DataType () {}

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

const DataType& findDataType(const std::string& name);
