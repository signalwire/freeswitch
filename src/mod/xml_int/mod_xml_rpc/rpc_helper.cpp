#include <string>
#include <set>
#include <vector>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "rpc_helper.h"

static int MIN_IDLE_CPU = 0;
static std::set<std::string> throttle_api_calls;

template <typename T>
T string_to_number(const std::string& str, T errorValue = 0)
  /// Convert a string to a numeric value
{
  try { return boost::lexical_cast<T>(str);} catch(...){return errorValue;};
}

static std::vector<std::string> string_tokenize(const std::string& str, const char* tok)
{
  std::vector<std::string> tokens;
  boost::split(tokens, str, boost::is_any_of(tok), boost::token_compress_on);
  return tokens;
}

static bool is_throttled_api(const std::string& api)
{
	for (std::set<std::string>::const_iterator iter = throttle_api_calls.begin(); iter != throttle_api_calls.end(); iter++)
	{
		if (api.find(*iter) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

void set_min_idle_cpu_watermark(const char* idle_cpu)
{
	MIN_IDLE_CPU = string_to_number<int>(idle_cpu);
}

switch_bool_t is_resource_available(const char* command, const char* api_str)
{
	std::string cmd(zstr(command) ? "" : command);
	std::string api(zstr(api_str) ? "" : api_str);
	double idle_cpu = switch_core_idle_cpu();
	if (api.empty() || throttle_api_calls.empty() || !MIN_IDLE_CPU)
	{
		return SWITCH_TRUE;
	}
	return (cmd == "bgapi" && is_throttled_api(api) && (idle_cpu) < MIN_IDLE_CPU) ? SWITCH_TRUE : SWITCH_FALSE;
}

void set_throttled_api_calls(const char* api)
{
	assert(!zstr(api));
	std::vector<std::string> tokens = string_tokenize(api, " ");
	for (std::vector<std::string>::const_iterator iter = tokens.begin(); iter != tokens.end(); iter++)
	{
		std::ostringstream strm;
		strm << " " << *iter << " ";
		throttle_api_calls.insert(strm.str());
	}
}