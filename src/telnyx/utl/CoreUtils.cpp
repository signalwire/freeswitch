// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/Telnyx.h"

#if TELNYX_OS_FAMILY_WINDOWS
  #include <Mmsystem.h>
  #include <winsock2.h>
  #include <windows.h>
  #include <stdio.h>
  #include <stdarg.h>
  #include <tchar.h>
  #include <process.h>
  #include <sys/types.h>
  #include <sys/timeb.h>
  #include <time.h>
  #include <stdlib.h>
#else
	#include <stdio.h>
	#include <stdarg.h>
	#include <unistd.h>
	#include <pthread.h>
	#include <sys/times.h>
	#include <limits.h>
	#include <sys/time.h>
	#include <time.h>
	#include <stdlib.h>
#endif

#include <string>
#include <sstream>
#include <locale>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include "Telnyx/UTL/CoreUtils.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/MD5Engine.h"
#include "Poco/File.h"
#include "Poco/DateTime.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"

#include "Telnyx/JSON/elements.h"
#include "Telnyx/JSON/reader.h"
#include "Telnyx/JSON/writer.h"

namespace Telnyx {

//
// Static dependencies
//
#if defined (TELNYX_OS_FAMILY_WINDOWS)
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "IPHlpApi.Lib") 
#pragma comment(lib, "Winmm.lib")
#endif

//
// String helper functions
//
bool string_starts_with(const std::string& str, const char* key)
{
  return str.find(key) == 0;
}

bool string_caseless_starts_with(const std::string& str, const char* key_)
{
  std::string key = key_;
  boost::to_lower(key);
  std::string lstr = str;
  boost::to_lower(lstr);
  return lstr.find(key) == 0; 
}

bool string_ends_with(const std::string& str, const char* key)
{
  size_t i = str.rfind(key);
  return (i != std::string::npos) && (i == (str.length() - ::strlen(key)));
}

bool string_caseless_ends_with(const std::string& str, const char* key)
{
  std::string lstr = str;
  boost::to_lower(lstr);
  size_t i = lstr.rfind(key);
  return (i != std::string::npos) && (i == (lstr.length() - ::strlen(key)));
}

void string_to_lower(std::string& str)
{
  boost::to_lower(str);
}

void string_to_upper(std::string& str)
{
  boost::to_upper(str);
}

std::string string_left(const std::string& str, size_t size)
{
  if (str.size() <= size)
    return str;
  return str.substr(0, size);
}

std::string string_left_until(const std::string& str, const char* key)
{
  size_t i = str.find(key);
  if (i != std::string::npos)
    return  str.substr(0, i);
  return str;
}

std::string string_right(const std::string& str, size_t size)
{
  if (str.size() <= size)
    return str;

  size_t index = str.size() - size;

  return str.substr(index, size);
}

void string_replace(std::string& str, const char* what, const char* with)
{
  size_t pos = 0;
  while((pos = str.find(what, pos)) != std::string::npos)
  {
     str.replace(pos, strlen(what), with);
     pos += strlen(with);
  }
}

std::vector<std::string> string_tokenize(const std::string& str, const char* tok)
{
  std::vector<std::string> tokens;
  boost::split(tokens, str, boost::is_any_of(tok), boost::token_compress_on);
  return tokens;
}

std::string string_create_uuid(bool random)
{
  if (random)
  {
    Poco::UUID uuid = Poco::UUIDGenerator::defaultGenerator().createRandom();
    return uuid.toString();
  }
  else
  {
    Poco::UUID uuid = Poco::UUIDGenerator::defaultGenerator().createOne();
    return uuid.toString();
  }
}

unsigned int string_to_js_hash(const std::string& str)
  /// A bitwise hash function written by Justin Sobel
{
  unsigned int hash = 1315423911;
  for(std::size_t i = 0; i < str.length(); i++)
  {
    hash ^= ((hash << 5) + str[i] + (hash >> 2));
  }
  return hash;
}

std::string string_to_64_bit_hash(const std::string& id)
{
  unsigned int hash = string_to_js_hash(id);
  std::ostringstream newId;
  newId << std::setfill('0') << std::hex << std::setw(8)
       << hash
       << std::setfill(' ') << std::dec;
  return newId.str();
}

//
// Misc functions
//

Telnyx::UInt64 getTicks()
{
#if TELNYX_OS_FAMILY_WINDOWS
  return (Telnyx::UInt64)timeGetTime();
#elif TELNYX_OS == TELNYX_OS_LINUX
	struct tms sTMS;
	clock_t ticks = times( &sTMS );
  return (Telnyx::UInt64)( ticks * ( 1000.0 / sysconf( _SC_CLK_TCK ) ) );
#elif TELNYX_OS == TELNYX_OS_SOLARIS
	hrtime_t tTime = gethrtime();
	return (Telnyx::UInt64)( tTime / 1000000 );
#else
  //
  //Find something for other OS
  //
  return 0;
#endif
}

int getRandom()
{
#if TELNYX_OS_FAMILY_WINDOWS
	static bool bInit = false;
	if ( !bInit )
	{
		srand( (unsigned int)::time( 0 ) + (unsigned int)getTicks() );
		bInit = true;
	}
	return rand();
#else
	static bool bInit = false;
	static unsigned int uSeed;
	if ( !bInit )
	{
		uSeed = (unsigned int)::time( 0 ) + (unsigned int)getTicks();
		bInit = true;
	}
	return rand_r( &uSeed );
#endif
}

Telnyx::UInt64 getTime()
{	
#if TELNYX_OS_FAMILY_WINDOWS
	struct _timeb sTimeB;
	_ftime( &sTimeB );
	return (Telnyx::UInt64)( sTimeB.time * 1000 + sTimeB.millitm );
#else
	struct timeval sTimeVal;
	gettimeofday( &sTimeVal, NULL );
	return (Telnyx::UInt64)( sTimeVal.tv_sec * 1000 + ( sTimeVal.tv_usec / 1000 ) );
#endif
}

tm* getTimeTS(Telnyx::UInt64 millis)
{
  tm* ret = 0;
  time_t seconds = (time_t)(millis/1000);
  if ((unsigned long long)seconds*1000 == millis)
    ret = localtime(&seconds);
  return ret; // milliseconds >= 4G*1000
}

std::string formatTime(tm* time, const char* fmt)
{
  char buff[256];
  strftime(buff, sizeof(buff)-1, fmt, time);
  return buff;
}

std::string boost_format_time(boost::posix_time::ptime now, const std::string& format)
{
  static std::locale loc(std::cout.getloc(), new boost::posix_time::time_facet(format.c_str()));
  std::ostringstream strTime;
  strTime.imbue(loc);
  strTime << now;
  return strTime.str();
}

std::string string_md5_hash(const char* input)
{
  TELNYX_VERIFY_NULL(input);
  TELNYX_VERIFY((*input) != 0x00);
  Poco::MD5Engine engine;
  engine.update(input, ::strlen(input));
  return Poco::MD5Engine::digestToHex(engine.digest());
}

size_t TELNYX_API string_hash(const char* str)
{
  static boost::hash<std::string> str_hash;
  return str_hash(str);
}

bool string_wildcard_compare(const char* wild, const std::string& str)
{
  const char* string = str.c_str();

  const char *cp = NULL, *mp = NULL;

  while ((*string) && (*wild != '*')) {
    if ((*wild != *string) && (*wild != '?')) {
      return 0;
    }
    wild++;
    string++;
  }

  while (*string) {
    if (*wild == '*') {
      if (!*++wild) {
        return 1;
      }
      mp = wild;
      cp = string+1;
    } else if ((*wild == *string) || (*wild == '?')) {
      wild++;
      string++;
    } else {
      wild = mp;
      string = cp++;
    }
  }

  while (*wild == '*') {
    wild++;
  }
  return !*wild;
}

void string_trim(std::string& str)
{
  std::locale loc;
  std::string::iterator front = str.begin();
  while (front != str.end())
  {
    if (std::isspace(*front, loc))
      front = str.erase(front);
    else
      break;
  }

  std::string::iterator back = str.end();
  while (back != str.begin())
  {
    back--;
    if (std::isspace(*back, loc))
    {
      str.erase(back);
      back = str.end();
    }
    else
      break;
  }
}

bool string_format_json(std::string& str)
{
  try
  {
    std::stringstream input;
    input << str;

    json::Object object;
    json::Reader::Read(object, input);

    std::stringstream output;
    json::Writer::Write(object, output);
    str = output.str();
    return true;
  }
  catch(const std::exception& e)
  {
    return false;
  }
}

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string string_base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';

  }

  return ret;

}

std::string string_base64_decode(std::string const& encoded_string) {
  size_t in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}



bool isFileOlderThan(const boost::filesystem::path& file, int minutes)
{
  Poco::File f(Telnyx::boost_path(file));
  Poco::Timestamp stamp = f.getLastModified();
  Poco::DateTime modified(stamp);
  Poco::DateTime now;
  Poco::Timespan elapsed = now - modified;
  return elapsed.totalMinutes() > minutes;
}

std::string boost_file_name(const boost::filesystem::path& path)
{
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
  return path.filename().native();
#else
  return path.filename();
#endif
}

std::string boost_path(const boost::filesystem::path& path)
{
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
  return path.native();
#else
  return path.string();
#endif
}

boost::filesystem::path boost_path_concatenate(const boost::filesystem::path& path, const std::string& file)
{
  return operator/(path, file.c_str());
}

#if 0
bool boost_temp_file(std::string& tempfile)
{
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
  try
  {
    boost::filesystem::path tmp = boost::filesystem::temp_directory_path();
    boost::filesystem::path fn = boost::filesystem::unique_path();
    boost::filesystem::path tempPath = operator/(tmp, Telnyx::boost_file_name(fn));
    tempfile = Telnyx::boost_path(tempPath);
    return true;
  }
  catch(...)
  {
  }
  return false;
#else
  /*tempfile = Poco::Path::temp();
  tempfile += "/";
  tempfile += Poco::TemporaryFile::tempName();*/
  tempfile = Poco::TemporaryFile::tempName();
  return true;
#endif
}
#endif

bool boost_temp_file(std::string& tempfile)
{
  tempfile = Poco::TemporaryFile::tempName();
  return true;
}

void vectorToCArray(const std::vector<std::string>& args, char*** argv)
{
  *argv = (char**)std::malloc((args.size() + 1) * sizeof(char*));
  int i=0;
  for(std::vector<std::string>::const_iterator iter = args.begin();
      iter != args.end();
      iter++, ++i)
  {
    std::string arg = *iter;
    (*argv)[i] = (char*)std::malloc((arg.length()+1) * sizeof(char));
    std::strcpy((*argv)[i], arg.c_str());
  }
  (*argv)[args.size()] = NULL; // argv must be NULL terminated
}

void freeCArray(int argc, char*** argv)
{
  for (int i = 0; i < argc; i++)
    free((*argv)[i]);
  free(*argv);
}

} // OSS

