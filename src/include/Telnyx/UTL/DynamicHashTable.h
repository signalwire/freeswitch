#ifndef TELNYX_DynamicHashTable_H_INCLUDED
#define TELNYX_DynamicHashTable_H_INCLUDED


#include <vector>
#include <map>
#include <string>

#include <boost/any.hpp>
#include <boost/shared_ptr.hpp>

#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/Thread.h"


namespace Telnyx {

class TELNYX_API DynamicHashTable
{
public:
  typedef boost::shared_ptr<DynamicHashTable> Ptr;
  typedef std::map<std::string, std::string> StringToStringMap;
  typedef std::map<int, std::string> IntToStringMap;
  typedef std::map< std::string, std::vector<std::string> > StringToVectorStringMap;
  typedef std::map< std::string, std::map<std::string, std::string> > StringToMapStringMap;
  typedef std::map< int, std::map< std::string, std::string > > IntToMapStringMap;
  typedef std::map< int, std::vector< std::string > > IntToVectorStringMap;
  typedef std::map< std::string, boost::any > StringToAnyMap;
  typedef std::map< int, boost::any > IntToAnyMap;
  typedef std::map< std::string, std::vector< boost::any > > StringToVectorAnyMap;
  typedef std::map< int, std::vector< boost::any > > IntToVectorAnyMap;
  typedef Telnyx::mutex_read_lock ReadLock;
  typedef Telnyx::mutex_write_lock WriteLock;

  DynamicHashTable();

  ~DynamicHashTable();

  //
  // Basic String Keys
  //
  void putString(const std::string& key, const std::string& value);

  void putString(int key, const std::string& value);

  std::string getString(const std::string& key) const;

  std::string getString(int key) const;

  StringToStringMap getStringKeys(const char* wildCardMatch) const;

  void deleteString(const std::string& key);

  void deleteString(int key);

  int deleteMultipleString(const char* wildCardMatch);

  //
  // Vector of string Keys
  //
  void putStringVector(const std::string& key, const std::vector<std::string>& value);

  void putStringVector(int key, const std::vector<std::string>& value);

  std::vector<std::string> getStringVector(const std::string& key) const;

  std::vector<std::string> getStringVector(int key) const;

  StringToVectorStringMap getStringVectorKeys(const char* wildCardMatch) const;

  void deleteStringVector(const std::string& key);

  void deleteStringVector(int key);

  int deleteMultipleStringVector(const char* wildCardMatch);

  //
  // Map of string Keys
  //
  void putStringMap(const std::string& key, const std::map<std::string, std::string>& value);

  void putStringMap(int key, const std::map<std::string, std::string>& value);

  std::map<std::string, std::string> getStringMap(const std::string& key) const;

  std::map<std::string, std::string> getStringMap(int key) const;

  StringToMapStringMap getStringMapKeys(const char* wildCardMatch) const;

  void deleteStringMap(const std::string& key);

  void deleteStringMap(int key);

  int deleteMultipleStringMap(const char* wildCardMatch);

  //
  // Basic ANY keys
  //
  void putAny(const std::string& key, const boost::any& value);

  void putAny(int key, const  boost::any& value);

  boost::any getAny(const std::string& key) const;

  boost::any getAny(int key) const;

  StringToAnyMap getAnyKeys(const char* wildCardMatch) const;

  void deleteAny(const std::string& key);

  void deleteAny(int key);

  int deleteMultipleAny(const char* wildCardMatch);

  //
  // Vector of Any Keys
  //
  void putAnyVector(const std::string& key, const std::vector<boost::any>& value);

  void putAnyVector(int key, const std::vector<boost::any>& value);

  std::vector<boost::any> getAnyVector(const std::string& key) const;

  std::vector<boost::any> getAnyVector(int key) const;

  StringToVectorAnyMap getAnyVectorKeys(const char* wildCardMatch) const;

  void deleteAnyVector(const std::string& key);

  void deleteAnyVector(int key);

  int deleteMultipleAnyVector(const char* wildCardMatch);

  //
  // Common functions
  //

  void clearAll();

protected:
  
  StringToStringMap _stringToString;
  IntToStringMap _intToString;
  StringToVectorStringMap _stringToVectorString;
  IntToVectorStringMap _intToVectorString;
  StringToMapStringMap _stringToMapString;
  IntToMapStringMap _intToMapString;
  StringToAnyMap _stringToAny;
  IntToAnyMap _intToAny;
  StringToVectorAnyMap _stringToVectorAny;
  IntToVectorAnyMap _intToVectorAny;

  mutable Telnyx::mutex_read_write _rwMutexStringToString;
  mutable Telnyx::mutex_read_write _rwMutexIntToString;
  mutable Telnyx::mutex_read_write _rwMutexStringToVectorString;
  mutable Telnyx::mutex_read_write _rwMutexIntToVectorString;
  mutable Telnyx::mutex_read_write _rwMutexStringToMapString;
  mutable Telnyx::mutex_read_write _rwMutexIntToMapString;
  mutable Telnyx::mutex_read_write _rwMutexStringToAny;
  mutable Telnyx::mutex_read_write _rwMutexIntToAny;
  mutable Telnyx::mutex_read_write _rwMutexStringToVectorAny;
  mutable Telnyx::mutex_read_write _rwMutexIntToVectorAny;
};

} // OSS
#endif // TELNYX_DynamicHashTable_H_INCLUDED
