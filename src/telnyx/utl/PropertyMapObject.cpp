// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/UTL/PropertyMapObject.h"


namespace Telnyx {


PropertyMapObject::PropertyMapObject()
{
}

PropertyMapObject::PropertyMapObject(const PropertyMapObject& copy)
{
  Telnyx::mutex_read_lock lock(copy._internalPropertiesMutex);
  _internalProperties = copy._internalProperties;
}

PropertyMapObject& PropertyMapObject::operator = (const PropertyMapObject& copy)
{
  PropertyMapObject swappable(copy);
  swap(swappable);
  return *this;
}

void PropertyMapObject::swap(PropertyMapObject& copy)
{
  Telnyx::mutex_write_lock lock_theirs(copy._internalPropertiesMutex);
  Telnyx::mutex_write_lock lock_ours(_internalPropertiesMutex);
  std::swap(_internalProperties, copy._internalProperties);
}

void PropertyMapObject::setProperty(const std::string& property, const std::string& value)
{
  if (property.empty())
    return;
  
  Telnyx::mutex_write_lock lock(_internalPropertiesMutex);
  _internalProperties[property] = value;
}

bool PropertyMapObject::getProperty(const std::string&  property, std::string& value) const
{
  if (property.empty())
    return false;
  
  Telnyx::mutex_read_lock lock_theirs(_internalPropertiesMutex);
  InternalProperties::const_iterator iter = _internalProperties.find(property);
  if (iter != _internalProperties.end())
  {
    value = iter->second;
    return true;
  }
  return false;
}

std::string PropertyMapObject::getProperty(const std::string&  property) const
{
  std::string value;
  getProperty(property, value);
  return value;
}

void PropertyMapObject::clearProperties()
{
  Telnyx::mutex_write_lock lock(_internalPropertiesMutex);
  _internalProperties.clear();
}

} // OSS


