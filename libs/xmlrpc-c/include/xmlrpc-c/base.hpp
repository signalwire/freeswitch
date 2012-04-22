#ifndef XMLRPC_BASE_HPP_INCLUDED
#define XMLRPC_BASE_HPP_INCLUDED

#include <climits>
#include <cfloat>
#include <ctime>
#include <vector>
#include <map>
#include <string>

#include <xmlrpc-c/base.h>

namespace xmlrpc_c {


class value {
    // This is a handle.  You don't want to create a pointer to this;
    // it is in fact a pointer itself.
public:
    value();
        // This creates a placeholder.  It can't be used for anything, but
        // holds memory.  instantiate() can turn it into a real object.

    value(xmlrpc_c::value const &value);  // copy constructor

    ~value();

    enum type_t {
        TYPE_INT        = 0,
        TYPE_BOOLEAN    = 1,
        TYPE_DOUBLE     = 2,
        TYPE_DATETIME   = 3,
        TYPE_STRING     = 4,
        TYPE_BYTESTRING = 5,
        TYPE_ARRAY      = 6,
        TYPE_STRUCT     = 7,
        TYPE_C_PTR      = 8,
        TYPE_NIL        = 9,
        TYPE_I8         = 10,
        TYPE_DEAD       = 0xDEAD
    };

    type_t type() const;

    xmlrpc_c::value&
    operator=(xmlrpc_c::value const&);

    bool
    isInstantiated() const;

    // The following are not meant to be public to users, but just to
    // other Xmlrpc-c library modules.  If we ever go to a pure C++
    // implementation, not based on C xmlrpc_value objects, this shouldn't
    // be necessary.

    void
    appendToCArray(xmlrpc_value * const arrayP) const;

    void
    addToCStruct(xmlrpc_value * const structP,
                 std::string    const key) const;

    xmlrpc_value *
    cValue() const;

    value(xmlrpc_value * const valueP);

    void
    instantiate(xmlrpc_value * const valueP);
        // Works only on a placeholder object created by the no-argument
        // constructor.

    xmlrpc_value * cValueP;
        // NULL means this is merely a placeholder object.
};



class value_int : public value {
public:
    value_int(int const cvalue);

    value_int(xmlrpc_c::value const baseValue);

    operator int() const;
};



class value_boolean : public value {
public:
    value_boolean(bool const cvalue);

    value_boolean(xmlrpc_c::value const baseValue);

    operator bool() const;
};



class value_string : public value {
public:
    enum nlCode {nlCode_all, nlCode_lf};

    value_string(std::string const& cppvalue,
                 nlCode      const  nlCode);

    value_string(std::string const& cppvalue);

    value_string(xmlrpc_c::value const baseValue);

    std::string
    crlfValue() const;

    operator std::string() const;
};



class value_double : public value {
public:
    value_double(double const cvalue);

    value_double(xmlrpc_c::value const baseValue);

    operator double() const;
};



class value_datetime : public value {
public:
    value_datetime(std::string const cvalue);
    value_datetime(time_t const cvalue);
#if XMLRPC_HAVE_TIMEVAL
    value_datetime(struct timeval const& cvalue);
#endif
#if XMLRPC_HAVE_TIMESPEC
    value_datetime(struct timespec const& cvalue);
#endif

    value_datetime(xmlrpc_c::value const baseValue);

    operator time_t() const;
};



class value_bytestring : public value {
public:
    value_bytestring(std::vector<unsigned char> const& cvalue);

    value_bytestring(xmlrpc_c::value const baseValue);

    // You can't cast to a vector because the compiler can't tell which
    // constructor to use (complains about ambiguity).  So we have this:
    std::vector<unsigned char>
    vectorUcharValue() const;

    size_t
    length() const;
};



class value_struct : public value {
public:
    value_struct(std::map<std::string, xmlrpc_c::value> const& cvalue);

    value_struct(xmlrpc_c::value const baseValue);

    operator std::map<std::string, xmlrpc_c::value>() const;
};



class value_array : public value {
public:
    value_array(std::vector<xmlrpc_c::value> const& cvalue);

    value_array(xmlrpc_c::value const baseValue);

    std::vector<xmlrpc_c::value>
    vectorValueValue() const;

    size_t
    size() const;
};



class value_nil : public value {
public:
    value_nil();

    value_nil(xmlrpc_c::value const baseValue);
};



class value_i8 : public value {
public:
    value_i8(xmlrpc_int64 const cvalue);

    value_i8(xmlrpc_c::value const baseValue);

    operator xmlrpc_int64() const;
};



class fault {
/*----------------------------------------------------------------------------
   This is an XML-RPC fault.

   This object is not intended to be used to represent a fault in the
   execution of XML-RPC client/server software -- just a fault in an
   XML-RPC RPC as described by the XML-RPC spec.

   There is no way to represent "no fault" with this object.  The object is
   meaningful only in the context of some fault.
-----------------------------------------------------------------------------*/
public:
    enum code_t {
        CODE_UNSPECIFIED            =    0,
        CODE_INTERNAL               = -500,
        CODE_TYPE                   = -501,
        CODE_INDEX                  = -502,
        CODE_PARSE                  = -503,
        CODE_NETWORK                = -504,
        CODE_TIMEOUT                = -505,
        CODE_NO_SUCH_METHOD         = -506,
        CODE_REQUEST_REFUSED        = -507,
        CODE_INTROSPECTION_DISABLED = -508,
        CODE_LIMIT_EXCEEDED         = -509,
        CODE_INVALID_UTF8           = -510
    };

    fault();

    fault(std::string             const _faultString,
          xmlrpc_c::fault::code_t const _faultCode 
              = xmlrpc_c::fault::CODE_UNSPECIFIED
        );
    
    xmlrpc_c::fault::code_t getCode() const;

    std::string getDescription() const;

private:
    bool                    valid;
    xmlrpc_c::fault::code_t code;
    std::string             description;
};

class rpcOutcome {
/*----------------------------------------------------------------------------
  The outcome of a validly executed RPC -- either an XML-RPC fault
  or an XML-RPC value of the result.
-----------------------------------------------------------------------------*/
public:
    rpcOutcome();
    rpcOutcome(xmlrpc_c::value const result);
    rpcOutcome(xmlrpc_c::fault const fault);
    bool succeeded() const;
    xmlrpc_c::fault getFault() const;
    xmlrpc_c::value getResult() const;
private:
    bool valid;
        // This is false in a placeholder variable -- i.e. an object you
        // create with the no-argument constructor, which is waiting to be
        // assigned a value.  When false, nothing below is valid.
    bool _succeeded;
    xmlrpc_c::value result;  // valid if 'succeeded'
    xmlrpc_c::fault fault;   // valid if not 'succeeded'
};

class paramList {
/*----------------------------------------------------------------------------
   A parameter list of an XML-RPC call.
-----------------------------------------------------------------------------*/
public:
    paramList(unsigned int const paramCount = 0);

    paramList&
    add(xmlrpc_c::value const param);

    paramList&
    addx(xmlrpc_c::value const param);

    unsigned int
    size() const;

    xmlrpc_c::value operator[](unsigned int const subscript) const;

    int
    getInt(unsigned int const paramNumber,
           int          const minimum = INT_MIN,
           int          const maximum = INT_MAX) const;

    bool
    getBoolean(unsigned int const paramNumber) const;

    double
    getDouble(unsigned int const paramNumber,
              double       const minimum = -DBL_MAX,
              double       const maximum = DBL_MAX) const;

    enum timeConstraint {TC_ANY, TC_NO_PAST, TC_NO_FUTURE};

    time_t
    getDatetime_sec(unsigned int   const paramNumber,
                    timeConstraint const constraint
                        = paramList::TC_ANY) const;

    std::string
    getString(unsigned int const paramNumber) const;

    std::vector<unsigned char>
    getBytestring(unsigned int const paramNumber) const;

    std::vector<xmlrpc_c::value>
    getArray(unsigned int const paramNumber,
             unsigned int const minSize = 0,
             unsigned int const maxSize = UINT_MAX) const;

    std::map<std::string, xmlrpc_c::value>
    getStruct(unsigned int const paramNumber) const;

    void
    getNil(unsigned int const paramNumber) const;

    xmlrpc_int64
    getI8(unsigned int const paramNumber,
          xmlrpc_int64 const minimum = XMLRPC_INT64_MIN,
          xmlrpc_int64 const maximum = XMLRPC_INT64_MAX) const;

    void
    verifyEnd(unsigned int const paramNumber) const;

private:
    std::vector<xmlrpc_c::value> paramVector;
};

} // namespace

#endif
