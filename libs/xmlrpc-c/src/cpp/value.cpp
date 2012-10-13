/*****************************************************************************
                                value.cpp
******************************************************************************
  This module provides services for dealing with XML-RPC values.  Each
  type of XML-RPC value is a C++ class.  An object represents a
  particular XML-RPC value.

  Everything is based on the C services in libxmlrpc.

  We could make things more efficient by using the internal interfaces
  via xmlrpc_int.h.  We could make them even more efficient by dumping
  libxmlrpc altogether for some or all of these services.

  An xmlrpc_c::value object is really just a handle for a C xmlrpc_value
  object.  You're not supposed to make a pointer to an xmlrpc_c::value
  object, but rather copy the object around.

  Because the C xmlrpc_value object does reference counting, it
  disappears automatically when the last handle does.  To go pure C++,
  we'd have to have a C++ object for the value itself and a separate
  handle object, like Boost's shared_ptr<>.

  The C++ is designed so that the user never sees the C interface at
  all.  Unfortunately, the user can see it if he wants because some
  class members had to be declared public so that other components of
  the library could see them, but the user is not supposed to access
  those members.
*****************************************************************************/

#include <cstdlib>
#include <string>
#include <vector>
#include <ctime>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "env_wrap.hpp"

#include "xmlrpc-c/base.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace {

void
throwIfError(env_wrap const& env) {

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



class cDatetimeValueWrapper {
public:
    xmlrpc_value * valueP;
    
    cDatetimeValueWrapper(time_t const cppvalue) {
        env_wrap env;
        
        this->valueP = xmlrpc_datetime_new_sec(&env.env_c, cppvalue);
        throwIfError(env);
    }
#if XMLRPC_HAVE_TIMEVAL
    cDatetimeValueWrapper(struct timeval const cppvalue) {
        env_wrap env;
        
        this->valueP = xmlrpc_datetime_new_timeval(&env.env_c, cppvalue);
        throwIfError(env);
    }
#endif
#if XMLRPC_HAVE_TIMESPEC
    cDatetimeValueWrapper(struct timespec const cppvalue) {
        env_wrap env;
        
        this->valueP = xmlrpc_datetime_new_timespec(&env.env_c, cppvalue);
        throwIfError(env);
    }
#endif
    ~cDatetimeValueWrapper() {
        xmlrpc_DECREF(this->valueP);
    }
};


class cStringWrapper {
public:
    const char * str;
    size_t length;
    cStringWrapper(xmlrpc_value * valueP) {
        env_wrap env;

        xmlrpc_read_string_lp(&env.env_c, valueP, &length, &str);
        throwIfError(env);
    }
    ~cStringWrapper() {
        free((char*)str);
    }
};



} // namespace



namespace xmlrpc_c {

value::value() {   // default constructor
    this->cValueP = NULL;
}



value::value(xmlrpc_value * const valueP) {

    this->instantiate(valueP);
}



value::value(xmlrpc_c::value const& value) {  // copy constructor
    this->cValueP = value.cValue();
}



xmlrpc_c::value&
value::operator=(xmlrpc_c::value const& value) {

    if (this->cValueP != NULL)
        throw(error("Assigning to already instantiated xmlrpc_c::value"));

    this->cValueP = value.cValue();
    return *this;  // The result of the (a = b) expression
}



value::~value() {
    if (this->cValueP) {
        xmlrpc_DECREF(this->cValueP);
    }
}



bool
value::isInstantiated() const {
/*----------------------------------------------------------------------------
   Return whether the object is actually a value, as opposed to a placeholder
   variable waiting to be assigned a value.
-----------------------------------------------------------------------------*/
    return (this->cValueP != NULL);
}



void
value::validateInstantiated() const {    // private
/*----------------------------------------------------------------------------
   Throw an exception if the object is just a placeholder, rather than an
   actual XML-RPC value.
-----------------------------------------------------------------------------*/
    if (!this->cValueP)
        throw(error("Reference to xmlrpc_c::value that has not been "
                    "instantiated.  (xmlrpc_c::value::isInstantiated may be "
                    "useful in diagnosing)"));
}



void
value::instantiate(xmlrpc_value * const valueP) {

    xmlrpc_INCREF(valueP);
    this->cValueP = valueP;
}



xmlrpc_value *
value::cValue() const {

    if (this->cValueP) {
        xmlrpc_INCREF(this->cValueP);  // For Caller
    }
    return this->cValueP;
}



void
value::appendToCArray(xmlrpc_value * const arrayP) const {
/*----------------------------------------------------------------------------
  Append this value to the C array 'arrayP'.
----------------------------------------------------------------------------*/
    this->validateInstantiated();

    env_wrap env;

    xmlrpc_array_append_item(&env.env_c, arrayP, this->cValueP);

    throwIfError(env);
}



void
value::addToCStruct(xmlrpc_value * const structP,
                    string         const key) const {
/*----------------------------------------------------------------------------
  Add this value to the C array 'arrayP' with key 'key'.
----------------------------------------------------------------------------*/
    this->validateInstantiated();

    env_wrap env;

    xmlrpc_struct_set_value_n(&env.env_c, structP,
                              key.c_str(), key.length(),
                              this->cValueP);

    throwIfError(env);
}



value::type_t 
value::type() const {

    this->validateInstantiated();

    /* You'd think we could just cast from xmlrpc_type to
       value::type_t, but Gcc warns if we do that.  So we have to do this
       even messier union nonsense.
    */
    union {
        xmlrpc_type   x;
        value::type_t y;
    } u;

    u.x = xmlrpc_value_type(this->cValueP);

    return u.y;
}



ostream& operator<<(ostream& out, value::type_t const& type) {

    string typeName;

    return out << string(xmlrpc_type_name((xmlrpc_type)type));
}



value_int::value_int(int const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(int const cppvalue) {
            env_wrap env;
            
            this->valueP = xmlrpc_int_new(&env.env_c, cppvalue);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_int::value_int(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_INT)
        throw(error("Not integer type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



value_int::operator int() const {

    this->validateInstantiated();

    int retval;
    env_wrap env;

    xmlrpc_read_int(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return retval;
}



int
value_int::cvalue() const {

    return static_cast<int>(*this);
}



value_double::value_double(double const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(double const cppvalue) {
            env_wrap env;
            
            this->valueP = xmlrpc_double_new(&env.env_c, cppvalue);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    this->instantiate(cWrapper(cppvalue).valueP);
}



value_double::value_double(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_DOUBLE)
        throw(error("Not double type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



value_double::operator double() const {

    this->validateInstantiated();

    double retval;

    env_wrap env;

    xmlrpc_read_double(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return retval;
}



double
value_double::cvalue() const {

    return static_cast<double>(*this);
}



value_boolean::value_boolean(bool const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(xmlrpc_bool const cppvalue) {
            env_wrap env;
            
            this->valueP = xmlrpc_bool_new(&env.env_c, cppvalue);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_boolean::value_boolean(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_BOOLEAN)
        throw(error("Not boolean type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



value_boolean::operator bool() const {

    this->validateInstantiated();

    xmlrpc_bool retval;

    env_wrap env;

    xmlrpc_read_bool(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return (retval != false);
}



bool
value_boolean::cvalue() const {

    return static_cast<bool>(*this);
}



value_datetime::value_datetime(string const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(string const cppvalue) {
            env_wrap env;
            
            this->valueP = xmlrpc_datetime_new_str(&env.env_c,
                                                   cppvalue.c_str());
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_datetime::value_datetime(time_t const cppvalue) {

    cDatetimeValueWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



#if XMLRPC_HAVE_TIMEVAL
value_datetime::value_datetime(struct timeval const& cppvalue) {

    cDatetimeValueWrapper wrapper(cppvalue);

    this->instantiate(wrapper.valueP);
}
#endif



#if XMLRPC_HAVE_TIMESPEC
value_datetime::value_datetime(struct timespec const& cppvalue) {

    cDatetimeValueWrapper wrapper(cppvalue);

    this->instantiate(wrapper.valueP);
}
#endif



value_datetime::value_datetime(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_DATETIME)
        throw(error("Not datetime type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



value_datetime::operator time_t() const {

    this->validateInstantiated();

    time_t retval;
    env_wrap env;

    xmlrpc_read_datetime_sec(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return retval;
}



#if XMLRPC_HAVE_TIMEVAL

value_datetime::operator timeval() const {

    this->validateInstantiated();

    struct timeval retval;
    env_wrap env;

    xmlrpc_read_datetime_timeval(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return retval;
}
#endif



#if XMLRPC_HAVE_TIMESPEC

value_datetime::operator timespec() const {

    this->validateInstantiated();

    struct timespec retval;
    env_wrap env;

    xmlrpc_read_datetime_timespec(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return retval;
}
#endif



time_t
value_datetime::cvalue() const {

    return static_cast<time_t>(*this);
}



class cNewStringWrapper {
public:
    xmlrpc_value * valueP;
        
    cNewStringWrapper(string               const cppvalue,
                      value_string::nlCode const nlCode) {
        env_wrap env;
            
        switch (nlCode) {
        case value_string::nlCode_all:
            this->valueP = xmlrpc_string_new_lp(&env.env_c,
                                                cppvalue.length(),
                                                cppvalue.c_str());
            break;
        case value_string::nlCode_lf:
            this->valueP = xmlrpc_string_new_lp_cr(&env.env_c,
                                                   cppvalue.length(),
                                                   cppvalue.c_str());
            break;
        default:
            throw(error("Newline encoding argument to value_string "
                        "constructor is not one of the defined "
                        "enumerations of value_string::nlCode"));
        }
        throwIfError(env);
    }
    ~cNewStringWrapper() {
        xmlrpc_DECREF(this->valueP);
    }
};
    


value_string::value_string(std::string          const& cppvalue,
                           value_string::nlCode const  nlCode) {
    
    cNewStringWrapper wrapper(cppvalue, nlCode);
    
    this->instantiate(wrapper.valueP);
}



value_string::value_string(std::string const& cppvalue) {

    cNewStringWrapper wrapper(cppvalue, nlCode_all);
    
    this->instantiate(wrapper.valueP);
}


    
value_string::value_string(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_STRING)
        throw(error("Not string type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



std::string
value_string::crlfValue() const {

    class cWrapper {
    public:
        const char * str;
        size_t length;
        cWrapper(xmlrpc_value * valueP) {
            env_wrap env;
            
            xmlrpc_read_string_lp_crlf(&env.env_c, valueP, &length, &str);
            throwIfError(env);
        }
        ~cWrapper() {
            free((char*)str);
        }
    };
    
    this->validateInstantiated();

    cWrapper wrapper(this->cValueP);

    return string(wrapper.str, wrapper.length);
}



value_string::operator string() const {

    this->validateInstantiated();

    cStringWrapper adapter(this->cValueP);

    return string(adapter.str, adapter.length);
}



std::string
value_string::cvalue() const {

    return static_cast<std::string>(*this);
}



value_bytestring::value_bytestring(
    vector<unsigned char> const& cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(vector<unsigned char> const& cppvalue) {
            env_wrap env;
            
            this->valueP = 
                xmlrpc_base64_new(&env.env_c, cppvalue.size(), &cppvalue[0]);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_bytestring::value_bytestring(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_BYTESTRING)
        throw(error("Not byte string type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



vector<unsigned char>
value_bytestring::vectorUcharValue() const {

    class cWrapper {
    public:
        const unsigned char * contents;
        size_t length;

        cWrapper(xmlrpc_value * const valueP) {
            env_wrap env;

            xmlrpc_read_base64(&env.env_c, valueP, &length, &contents);
            throwIfError(env);
        }
        ~cWrapper() {
            free((void*)contents);
        }
    };
    
    this->validateInstantiated();

    cWrapper wrapper(this->cValueP);

    return vector<unsigned char>(&wrapper.contents[0], 
                                 &wrapper.contents[wrapper.length]);
}



vector<unsigned char>
value_bytestring::cvalue() const {

    return this->vectorUcharValue();
}



size_t
value_bytestring::length() const {

    this->validateInstantiated();

    env_wrap env;
    size_t length;

    xmlrpc_read_base64_size(&env.env_c, this->cValueP, &length);
    throwIfError(env);

    return length;
}



value_array::value_array(vector<xmlrpc_c::value> const& cppvalue) {
    
    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper() {
            env_wrap env;
            
            this->valueP = xmlrpc_array_new(&env.env_c);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper;
    
    vector<xmlrpc_c::value>::const_iterator i;
    for (i = cppvalue.begin(); i != cppvalue.end(); ++i)
        i->appendToCArray(wrapper.valueP);

    this->instantiate(wrapper.valueP);
}



value_array::value_array(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_ARRAY)
        throw(error("Not array type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



vector<xmlrpc_c::value>
value_array::vectorValueValue() const {

    this->validateInstantiated();

    env_wrap env;

    unsigned int arraySize;

    arraySize = xmlrpc_array_size(&env.env_c, this->cValueP);
    throwIfError(env);
    
    vector<xmlrpc_c::value> retval(arraySize);
    
    for (unsigned int i = 0; i < arraySize; ++i) {

        class cWrapper {
        public:
            xmlrpc_value * valueP;

            cWrapper(xmlrpc_value * const arrayP,
                     unsigned int   const index) {
                env_wrap env;

                xmlrpc_array_read_item(&env.env_c, arrayP, index, &valueP);
                
                throwIfError(env);
            }
            ~cWrapper() {
                xmlrpc_DECREF(valueP);
            }
        };

        cWrapper wrapper(this->cValueP, i);

        retval[i].instantiate(wrapper.valueP);
    }

    return retval;
}



vector<xmlrpc_c::value>
value_array::cvalue() const {

    return this->vectorValueValue();
}



size_t
value_array::size() const {

    this->validateInstantiated();

    env_wrap env;
    unsigned int arraySize;

    arraySize = xmlrpc_array_size(&env.env_c, this->cValueP);
    throwIfError(env);
    
    return arraySize;
}



value_struct::value_struct(
    map<string, xmlrpc_c::value> const &cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper() {
            env_wrap env;
            
            this->valueP = xmlrpc_struct_new(&env.env_c);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper;

    map<string, xmlrpc_c::value>::const_iterator i;
    for (i = cppvalue.begin(); i != cppvalue.end(); ++i) {
        xmlrpc_c::value mapvalue(i->second);
        string          mapkey(i->first);
        mapvalue.addToCStruct(wrapper.valueP, mapkey);
    }
    
    this->instantiate(wrapper.valueP);
}



value_struct::value_struct(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_STRUCT)
        throw(error("Not struct type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



value_struct::operator map<string, xmlrpc_c::value>() const {

    this->validateInstantiated();

    env_wrap env;
    unsigned int structSize;

    structSize = xmlrpc_struct_size(&env.env_c, this->cValueP);
    throwIfError(env);
    
    map<string, xmlrpc_c::value> retval;
    
    for (unsigned int i = 0; i < structSize; ++i) {
        class cMemberWrapper {
        public:
            xmlrpc_value * keyP;
            xmlrpc_value * valueP;

            cMemberWrapper(xmlrpc_value * const structP,
                           unsigned int   const index) {

                env_wrap env;
            
                xmlrpc_struct_read_member(&env.env_c, structP, index, 
                                          &keyP, &valueP);
                
                throwIfError(env);
            }
            ~cMemberWrapper() {
                xmlrpc_DECREF(keyP);
                xmlrpc_DECREF(valueP);
            }
        };

        cMemberWrapper memberWrapper(this->cValueP, i);

        cStringWrapper keyWrapper(memberWrapper.keyP);

        string const key(keyWrapper.str, keyWrapper.length);

        retval[key] = xmlrpc_c::value(memberWrapper.valueP);
    }

    return retval;
}



map<string, xmlrpc_c::value>
value_struct::cvalue() const {

    return static_cast<map<string, xmlrpc_c::value> >(*this);
}



value_nil::value_nil() {
    
    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper() {
            env_wrap env;
            
            this->valueP = xmlrpc_nil_new(&env.env_c);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper;
    
    this->instantiate(wrapper.valueP);
}
    


value_nil::value_nil(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_NIL)
        throw(error("Not nil type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



void *
value_nil::cvalue() const {

    return NULL;
}



value_i8::value_i8(xmlrpc_int64 const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(xmlrpc_int64 const cppvalue) {
            env_wrap env;
            
            this->valueP = xmlrpc_i8_new(&env.env_c, cppvalue);
            throwIfError(env);
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_i8::value_i8(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_I8)
        throw(error("Not 64 bit integer type.  See type() method"));
    else {
        this->instantiate(baseValue.cValueP);
    }
}



value_i8::operator xmlrpc_int64() const {

    this->validateInstantiated();

    xmlrpc_int64 retval;
    env_wrap env;

    xmlrpc_read_i8(&env.env_c, this->cValueP, &retval);
    throwIfError(env);

    return retval;
}



xmlrpc_int64
value_i8::cvalue() const {

    return static_cast<xmlrpc_int64>(*this);
}



} // namespace
