// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/UTL/DynamicHashTable.h"

namespace Telnyx {


DynamicHashTable::DynamicHashTable()
{
}

DynamicHashTable::~DynamicHashTable()
{
}

void DynamicHashTable::putString(const std::string& key, const std::string& value)
{
  WriteLock lock(_rwMutexStringToString);
  _stringToString[key] = value;
}

void DynamicHashTable::putString(int key, const std::string& value)
{
  WriteLock lock(_rwMutexIntToString);
  _intToString[key] = value;;
}

std::string DynamicHashTable::getString(const std::string& key) const
{
  ReadLock lock(_rwMutexStringToString);
  StringToStringMap::const_iterator iter = _stringToString.find(key);
  if (iter != _stringToString.end())
    return iter->second;
  return "";
}

std::string DynamicHashTable::getString(int key) const
{
  ReadLock lock(_rwMutexIntToString);
  IntToStringMap::const_iterator iter = _intToString.find(key);
  if (iter != _intToString.end())
    return iter->second;
  return "";
}

DynamicHashTable::StringToStringMap DynamicHashTable::getStringKeys(const char* wildCardMatch) const
{
  ReadLock lock(_rwMutexStringToString);

  StringToStringMap subset;
  for (StringToStringMap::const_iterator iter = _stringToString.begin(); 
    iter != _stringToString.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
      subset[iter->first] = iter->second;
  }
  return subset;
}

void DynamicHashTable::deleteString(const std::string& key)
{
  WriteLock lock(_rwMutexStringToString);
  _stringToString.erase(key);
}

void DynamicHashTable::deleteString(int key)
{
  WriteLock lock(_rwMutexIntToString);
  _intToString.erase(key);
}

int DynamicHashTable::deleteMultipleString(const char* wildCardMatch)
{
  WriteLock lock(_rwMutexStringToString);
  std::vector<std::string> deleted;

  for (StringToStringMap::iterator iter = _stringToString.begin();
    iter != _stringToString.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
    {
      deleted.push_back(iter->first);
    }
  }

  for (std::vector<std::string>::iterator iter = deleted.begin(); iter != deleted.end(); iter++)
    _stringToString.erase(*iter);

  return deleted.size();
}

void DynamicHashTable::putStringVector(const std::string& key, const std::vector<std::string>& value)
{
  WriteLock lock(_rwMutexStringToVectorString);
  _stringToVectorString[key] = value;
}

void DynamicHashTable::putStringVector(int key, const std::vector<std::string>& value)
{
  WriteLock lock(_rwMutexIntToVectorString);
  _intToVectorString[key] = value;
}

std::vector<std::string> DynamicHashTable::getStringVector(const std::string& key) const
{
  ReadLock lock(_rwMutexStringToVectorString);
  StringToVectorStringMap::const_iterator iter = _stringToVectorString.find(key);
  if (iter != _stringToVectorString.end())
    return iter->second;
  return std::vector<std::string>();
}

std::vector<std::string> DynamicHashTable::getStringVector(int key) const
{
  ReadLock lock(_rwMutexIntToVectorString);
  IntToVectorStringMap::const_iterator iter = _intToVectorString.find(key);
  if (iter != _intToVectorString.end())
    return iter->second;
  return std::vector<std::string>();
}

DynamicHashTable::StringToVectorStringMap DynamicHashTable::getStringVectorKeys(const char* wildCardMatch) const
{
  ReadLock lock(_rwMutexStringToVectorString);
  StringToVectorStringMap subset;
  for (StringToVectorStringMap::const_iterator iter = _stringToVectorString.begin(); 
    iter != _stringToVectorString.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
      subset[iter->first] = iter->second;
  }
  return subset;
}

void DynamicHashTable::deleteStringVector(const std::string& key)
{
  WriteLock lock(_rwMutexStringToVectorString);
  _stringToVectorString.erase(key);
}

void DynamicHashTable::deleteStringVector(int key)
{
  WriteLock lock(_rwMutexStringToVectorString);
  _intToVectorString.erase(key);
}

int DynamicHashTable::deleteMultipleStringVector(const char* wildCardMatch)
{
  WriteLock lock(_rwMutexStringToVectorString);
  std::vector<std::string> deleted;

  for (StringToVectorStringMap::iterator iter = _stringToVectorString.begin();
    iter != _stringToVectorString.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
    {
      deleted.push_back(iter->first);
    }
  }

  for (std::vector<std::string>::iterator iter = deleted.begin(); iter != deleted.end(); iter++)
    _stringToVectorString.erase(*iter);

  return deleted.size();
}

void DynamicHashTable::putAny(const std::string& key, const boost::any& value)
{
  WriteLock lock(_rwMutexStringToAny);
  _stringToAny[key] = value;
}

void DynamicHashTable::putAny(int key, const  boost::any& value)
{
  WriteLock lock(_rwMutexIntToAny);
  _intToAny[key] = value;
}

boost::any DynamicHashTable::getAny(const std::string& key) const
{
  ReadLock lock(_rwMutexStringToAny);
  DynamicHashTable::StringToAnyMap::const_iterator iter = _stringToAny.find(key);
  if (iter != _stringToAny.end())
    return iter->second;
  return boost::any();
}

boost::any DynamicHashTable::getAny(int key) const
{
  ReadLock lock(_rwMutexIntToAny);
  DynamicHashTable::IntToAnyMap::const_iterator iter = _intToAny.find(key);
  if (iter != _intToAny.end())
    return iter->second;
  return boost::any();
}

DynamicHashTable::StringToAnyMap DynamicHashTable::getAnyKeys(const char* wildCardMatch) const
{
  ReadLock lock(_rwMutexStringToAny);

  StringToAnyMap subset;
  for (StringToAnyMap::const_iterator iter = _stringToAny.begin(); 
    iter != _stringToAny.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
      subset[iter->first] = iter->second;
  }
  return subset;
}

void DynamicHashTable::deleteAny(const std::string& key)
{
  WriteLock lock(_rwMutexStringToAny);
  _stringToAny.erase(key);
}

void DynamicHashTable::deleteAny(int key)
{
  WriteLock lock(_rwMutexStringToAny);
  _intToAny.erase(key);
}

int DynamicHashTable::deleteMultipleAny(const char* wildCardMatch)
{
  WriteLock lock(_rwMutexStringToAny);
  std::vector<std::string> deleted;

  for (StringToAnyMap::iterator iter = _stringToAny.begin();
    iter != _stringToAny.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
    {
      deleted.push_back(iter->first);
    }
  }

  for (std::vector<std::string>::iterator iter = deleted.begin(); iter != deleted.end(); iter++)
    _stringToAny.erase(*iter);

  return deleted.size();
}

void DynamicHashTable::putAnyVector(const std::string& key, const std::vector<boost::any>& value)
{
  WriteLock lock(_rwMutexStringToVectorAny);
  _stringToVectorAny[key] = value;
}

void DynamicHashTable::putAnyVector(int key, const std::vector<boost::any>& value)
{
  WriteLock lock(_rwMutexIntToVectorAny);
  _intToVectorAny[key] = value;
}

std::vector<boost::any> DynamicHashTable::getAnyVector(const std::string& key) const
{
  ReadLock lock(_rwMutexStringToVectorAny);
  DynamicHashTable::StringToVectorAnyMap::const_iterator iter = _stringToVectorAny.find(key);
  if (iter != _stringToVectorAny.end())
    return iter->second;
  return std::vector<boost::any>();
}

std::vector<boost::any> DynamicHashTable::getAnyVector(int key) const
{
  ReadLock lock(_rwMutexIntToVectorAny);
  DynamicHashTable::IntToVectorAnyMap::const_iterator iter = _intToVectorAny.find(key);
  if (iter != _intToVectorAny.end())
    return iter->second;
  return std::vector<boost::any>();
}

DynamicHashTable::StringToVectorAnyMap DynamicHashTable::getAnyVectorKeys(const char* wildCardMatch) const
{
  ReadLock lock(_rwMutexStringToVectorAny);
  StringToVectorAnyMap subset;
  for (StringToVectorAnyMap::const_iterator iter = _stringToVectorAny.begin(); 
    iter != _stringToVectorAny.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
      subset[iter->first] = iter->second;
  }
  return subset;
}

void DynamicHashTable::deleteAnyVector(const std::string& key)
{
  WriteLock lock(_rwMutexStringToVectorAny);
  _stringToVectorAny.erase(key);
}

void DynamicHashTable::deleteAnyVector(int key)
{
  WriteLock lock(_rwMutexIntToVectorAny);
  _intToVectorAny.erase(key);
}

int DynamicHashTable::deleteMultipleAnyVector(const char* wildCardMatch)
{
  WriteLock lock(_rwMutexStringToVectorAny);
  std::vector<std::string> deleted;

  for (StringToVectorAnyMap::iterator iter = _stringToVectorAny.begin();
    iter != _stringToVectorAny.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
    {
      deleted.push_back(iter->first);
    }
  }

  for (std::vector<std::string>::iterator iter = deleted.begin(); iter != deleted.end(); iter++)
    _stringToVectorAny.erase(*iter);

  return deleted.size();
}

void DynamicHashTable::clearAll()
{
  {
    WriteLock lock(_rwMutexStringToString);
    _stringToString.clear();
  }

  {
    WriteLock lock(_rwMutexIntToString);
    _intToString.clear();
  }

  {
    WriteLock lock(_rwMutexStringToVectorString);
    _stringToVectorString.clear();
  }

  {
    WriteLock lock(_rwMutexIntToVectorString);
    _intToVectorString.clear();
  }

  {
    WriteLock lock(_rwMutexStringToAny);
    _stringToAny.clear();
  }

  {
    WriteLock lock(_rwMutexIntToAny);
    _intToAny.clear();
  }

  {
    WriteLock lock(_rwMutexStringToVectorAny);
    _stringToVectorAny.clear();
  }

  {
    WriteLock lock(_rwMutexIntToVectorAny);
    _intToVectorAny.clear();
  }
}

//
//
//

void DynamicHashTable::putStringMap(const std::string& key, const std::map<std::string, std::string>& value)
{
  WriteLock lock(_rwMutexStringToMapString);
  _stringToMapString[key] = value;
}

void DynamicHashTable::putStringMap(int key, const std::map<std::string, std::string>& value)
{
  WriteLock lock(_rwMutexIntToMapString);
  _intToMapString[key] = value;
}

std::map<std::string, std::string> DynamicHashTable::getStringMap(const std::string& key) const
{
  ReadLock lock(_rwMutexStringToMapString);
  StringToMapStringMap::const_iterator iter = _stringToMapString.find(key);
  if (iter != _stringToMapString.end())
    return iter->second;
  return std::map<std::string, std::string>();
}

std::map<std::string, std::string> DynamicHashTable::getStringMap(int key) const
{
  ReadLock lock(_rwMutexIntToMapString);
  IntToMapStringMap::const_iterator iter = _intToMapString.find(key);
  if (iter != _intToMapString.end())
    return iter->second;
  return std::map<std::string, std::string>();
}

DynamicHashTable::StringToMapStringMap DynamicHashTable::getStringMapKeys(const char* wildCardMatch) const
{
  ReadLock lock(_rwMutexStringToMapString);
  StringToMapStringMap subset;
  for (StringToMapStringMap::const_iterator iter = _stringToMapString.begin(); 
    iter != _stringToMapString.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
      subset[iter->first] = iter->second;
  }
  return subset;
}

void DynamicHashTable::deleteStringMap(const std::string& key)
{
  WriteLock lock(_rwMutexStringToMapString);
  _stringToMapString.erase(key);
}

void DynamicHashTable::deleteStringMap(int key)
{
  WriteLock lock(_rwMutexStringToMapString);
  _intToMapString.erase(key);
}

int DynamicHashTable::deleteMultipleStringMap(const char* wildCardMatch)
{
  WriteLock lock(_rwMutexStringToMapString);
  std::vector<std::string> deleted;

  for (StringToMapStringMap::iterator iter = _stringToMapString.begin();
    iter != _stringToMapString.end(); iter++)
  {
    if (Telnyx::string_wildcard_compare(wildCardMatch, iter->first))
    {
      deleted.push_back(iter->first);
    }
  }

  for (std::vector<std::string>::iterator iter = deleted.begin(); iter != deleted.end(); iter++)
    _stringToMapString.erase(*iter);

  return deleted.size();
}


} // OSS



