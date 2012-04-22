#include <climits>
#include <cfloat>
#include <ctime>
#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {


paramList::paramList(unsigned int const paramCount) {

    this->paramVector.reserve(paramCount);
}


 
paramList&
paramList::add(xmlrpc_c::value const param) {

    // Note: Before Xmlrpc-c 1.10, the return value was void.  Old programs
    // using this new add() won't notice the difference.  New programs
    // using this new add() against an old library will, since the old
    // add() will not return anything.  A new program that wants to get
    // a link error instead of a crash in this case can use addx() instead.

    this->paramVector.push_back(param);

    return *this;
}



paramList&
paramList::addx(xmlrpc_c::value const param) {

    // See add() for an explanation of why this exists.

    return this->add(param);
}



unsigned int
paramList::size() const {
    return this->paramVector.size();
}



xmlrpc_c::value 
paramList::operator[](unsigned int const subscript) const {

    if (subscript >= this->paramVector.size())
        throw(girerr::error(
            "Subscript of xmlrpc_c::paramList out of bounds"));

    return this->paramVector[subscript];
}



int
paramList::getInt(unsigned int const paramNumber,
                  int          const minimum,
                  int          const maximum) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_INT)
        throw(fault("Parameter that is supposed to be integer is not", 
                    fault::CODE_TYPE));

    int const intvalue(static_cast<int>(
        value_int(this->paramVector[paramNumber])));

    if (intvalue < minimum)
        throw(fault("Integer parameter too low", fault::CODE_TYPE));

    if (intvalue > maximum)
        throw(fault("Integer parameter too high", fault::CODE_TYPE));

    return intvalue;
}



bool
paramList::getBoolean(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_BOOLEAN)
        throw(fault("Parameter that is supposed to be boolean is not", 
                    fault::CODE_TYPE));

    return static_cast<bool>(value_boolean(this->paramVector[paramNumber]));
}



double
paramList::getDouble(unsigned int const paramNumber,
                     double       const minimum,
                     double       const maximum) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_DOUBLE)
        throw(fault("Parameter that is supposed to be floating point number "
                    "is not", 
                    fault::CODE_TYPE));

    double const doublevalue(static_cast<double>(
        value_double(this->paramVector[paramNumber])));

    if (doublevalue < minimum)
        throw(fault("Floating point number parameter too low",
                    fault::CODE_TYPE));

    if (doublevalue > maximum)
        throw(fault("Floating point number parameter too high",
                    fault::CODE_TYPE));

    return doublevalue;
}



time_t
paramList::getDatetime_sec(
    unsigned int              const paramNumber,
    paramList::timeConstraint const constraint) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_DATETIME)
        throw(fault("Parameter that is supposed to be a datetime is not", 
                    fault::CODE_TYPE));

    time_t const timeValue(static_cast<time_t>(value_datetime(*paramP)));
    time_t const now(time(NULL));

    switch (constraint) {
    case TC_ANY:
        /* He'll take anything; no problem */
        break;
    case TC_NO_FUTURE:
        if (timeValue > now)
            throw(fault("Datetime parameter that is not supposed to be in "
                        "the future is.", fault::CODE_TYPE));
        break;
    case TC_NO_PAST:
        if (timeValue < now)
            throw(fault("Datetime parameter that is not supposed to be in "
                        "the past is.", fault::CODE_TYPE));
        break;
    }

    return timeValue;
}



string
paramList::getString(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_STRING)
        throw(fault("Parameter that is supposed to be a string is not", 
                    fault::CODE_TYPE));

    return static_cast<string>(value_string(this->paramVector[paramNumber]));
}



std::vector<unsigned char>
paramList::getBytestring(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_BYTESTRING)
        throw(fault("Parameter that is supposed to be a byte string is not", 
                    fault::CODE_TYPE));

    return value_bytestring(*paramP).vectorUcharValue();
}


std::vector<xmlrpc_c::value>
paramList::getArray(unsigned int const paramNumber,
                     unsigned int const minSize,
                     unsigned int const maxSize) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_ARRAY)
        throw(fault("Parameter that is supposed to be an array is not", 
                    fault::CODE_TYPE));

    xmlrpc_c::value_array const arrayValue(*paramP);
    
    if (arrayValue.size() < minSize)
        throw(fault("Array parameter has too few elements",
                    fault::CODE_TYPE));
    
    if (arrayValue.size() > maxSize)
        throw(fault("Array parameter has too many elements",
                    fault::CODE_TYPE));

    return value_array(*paramP).vectorValueValue();
}



std::map<string, xmlrpc_c::value>
paramList::getStruct(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_STRUCT)
        throw(fault("Parameter that is supposed to be a structure is not", 
                    fault::CODE_TYPE));

    return static_cast<std::map<string, xmlrpc_c::value> >(
        value_struct(*paramP));
}



void
paramList::getNil(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_NIL)
        throw(fault("Parameter that is supposed to be nil is not", 
                    fault::CODE_TYPE));
}



long long
paramList::getI8(unsigned int const paramNumber,
                 long long    const minimum,
                 long long    const maximum) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_I8)
        throw(fault("Parameter that is supposed to be 64-bit integer is not", 
                    fault::CODE_TYPE));

    long long const longlongvalue(static_cast<long long>(
        value_i8(this->paramVector[paramNumber])));

    if (longlongvalue < minimum)
        throw(fault("64-bit integer parameter too low", fault::CODE_TYPE));

    if (longlongvalue > maximum)
        throw(fault("64-bit integer parameter too high", fault::CODE_TYPE));

    return longlongvalue;
}



void
paramList::verifyEnd(unsigned int const paramNumber) const {

    if (paramNumber < this->paramVector.size())
        throw(fault("Too many parameters", fault::CODE_TYPE));
    if (paramNumber > this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));
}

}  // namespace
