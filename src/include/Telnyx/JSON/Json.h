// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_JSON_H_INCLUDED
#define TELNYX_JSON_H_INCLUDED

#include "Telnyx/JSON/reader.h"
#include "Telnyx/JSON/writer.h"

namespace Telnyx {
namespace JSON {
  
  
typedef json::UnknownElement UnknownElement;
typedef json::Array Array;
typedef json::Boolean Boolean;
typedef json::Number Number;
typedef json::Object Object;
typedef json::String String;
typedef json::Reader Reader;
typedef json::Writer Writer;
typedef json::Exception Exception;

bool json_parse_string(const std::string& jsonString, Telnyx::JSON::Object& object);
bool json_parse_string(const std::string& jsonString, Telnyx::JSON::Object& object, Telnyx::JSON::Exception& e);
bool json_object_to_string(const Telnyx::JSON::Object& object, std::string& jsonString);
bool json_object_to_string(const Telnyx::JSON::Object& object, std::string& jsonString, Telnyx::JSON::Exception& e);

template <typename T>
bool json_to_string(const T& object, std::string& jsonString, Telnyx::JSON::Exception& e)
{
  try
  {
    std::stringstream ostr;
    Telnyx::JSON::Writer::Write(object, ostr);
    jsonString = ostr.str();
  }
  catch(Telnyx::JSON::Exception& e_)
  {
    e = e_;
    return false;
  }
  return true;
}

template <typename T>
bool json_to_string(const T& object, std::string& jsonString)
{
  Telnyx::JSON::Exception e;
  return json_to_string<T>(object, jsonString, e);
}


  
} }

#endif // TELNYX_JSON_H_INCLUDED


