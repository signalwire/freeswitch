// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_PROPERTYMAPOBJECT_H_INCLUDED
#define TELNYX_PROPERTYMAPOBJECT_H_INCLUDED

#include "Telnyx/UTL/PropertyMap.h"
#include "Telnyx/UTL/Thread.h"
#include <map>
#include <string>

namespace Telnyx {

class PropertyMapObject
{
public:
  typedef std::map<std::string, std::string> InternalProperties;
  PropertyMapObject();
  PropertyMapObject(const PropertyMapObject& copy);
  PropertyMapObject& operator = (const PropertyMapObject& copy);
  void swap(PropertyMapObject& copy);
  
  void setProperty(const std::string& property, const std::string& value);
    /// Set a custom property for this object.
    /// Custom properties are meant to simply hold
    /// arbitrary data to aid in how the object
    /// is processed.
  
  void setProperty(PropertyMap::Enum property, const std::string& value);
    /// Set a custom property for this object.
    /// Custom properties are meant to simply hold
    /// arbitrary data to aid in how the object
    /// is processed.

  bool getProperty(const std::string&  property, std::string& value) const;
    /// Get a custom property of this object.
    /// Custom properties are meant to simply hold
    /// arbitrary data to aid in how the object
    /// is processed.
  
  bool getProperty(PropertyMap::Enum property, std::string& value) const;
    /// Get a custom property of this object.
    /// Custom properties are meant to simply hold
    /// arbitrary data to aid in how the object
    /// is processed.
  
  std::string getProperty(const std::string&  property) const;
  std::string getProperty(PropertyMap::Enum property) const;

  void clearProperties();
    /// Remove all custom properties
  
protected:
  InternalProperties _internalProperties;
  mutable Telnyx::mutex_read_write _internalPropertiesMutex;
};

//
// Inlines
//
inline bool PropertyMapObject::getProperty(PropertyMap::Enum   property, std::string& value) const
{
  return getProperty(PropertyMap::propertyString(property), value);
}

inline std::string PropertyMapObject::getProperty(PropertyMap::Enum property) const
{
  return getProperty(PropertyMap::propertyString(property));
}

inline void PropertyMapObject::setProperty(PropertyMap::Enum property, const std::string& value)
{
  setProperty(PropertyMap::propertyString(property), value);
}

} // OSS

#endif // TELNYX_PROPERTYMAPOBJECT_H_INCLUDED

