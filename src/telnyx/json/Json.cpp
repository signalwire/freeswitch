// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/JSON/Json.h"

namespace Telnyx {
namespace JSON {
  
  
bool json_parse_string(const std::string& jsonString, Telnyx::JSON::Object& object)
{
  Telnyx::JSON::Exception e;
  return json_parse_string(jsonString, object, e);
}

bool json_parse_string(const std::string& jsonString, Telnyx::JSON::Object& object, Telnyx::JSON::Exception& e)
{
  try
  {
    std::stringstream ostr;
    ostr << jsonString;
    Telnyx::JSON::Reader::Read(object, ostr);
  }
  catch(Telnyx::JSON::Exception& e_)
  {
    e = e_;
    return false;
  }
  return true;
}

bool json_object_to_string(const Telnyx::JSON::Object& object, std::string& jsonString)
{
  Telnyx::JSON::Exception e;
  return json_object_to_string(object, jsonString, e);
}

bool json_object_to_string(const Telnyx::JSON::Object& object, std::string& jsonString, Telnyx::JSON::Exception& e)
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


} } // Telnyx::JSON