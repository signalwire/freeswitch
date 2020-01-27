

#ifndef MOD_UTILS_H
#define MOD_UTILS_H

#include <switch.h>
#include <string>
#include <vector>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <sstream>

#include <execinfo.h>
#include <errno.h>
#include <cxxabi.h>
#include <dlfcn.h>     // for dladdr

#include <Telnyx/UTL/CoreUtils.h>
#include <Telnyx/UTL/Thread.h>

#define LOG(resource, level, msg) { std::ostringstream __strm__; __strm__ << msg << "\n"; switch_log_printf(resource, level, "%s", __strm__.str().c_str()); }
#define CHAN_LOG(level, msg) LOG(SWITCH_CHANNEL_LOG, level, msg)
#define CHAN_LOG_INFO(msg) CHAN_LOG(SWITCH_LOG_INFO, msg)
#define CHAN_LOG_DEBUG(msg) CHAN_LOG(SWITCH_LOG_DEBUG, msg)
#define CHAN_LOG_ERROR(msg) CHAN_LOG(SWITCH_LOG_ERROR, msg)
#define CHAN_LOG_NOTICE(msg) CHAN_LOG(SWITCH_LOG_NOTICE, msg)
#define CHAN_LOG_WARNING(msg) CHAN_LOG(SWITCH_LOG_WARNING, msg)
#define CHAN_LOG_CRIT(msg) CHAN_LOG(SWITCH_LOG_CRIT, msg)
#define SESS_LOG(session, level, msg) LOG(SWITCH_CHANNEL_SESSION_LOG(session), level, msg)
#define SESS_LOG_INFO(session,msg) SESS_LOG(session,SWITCH_LOG_INFO, msg)
#define SESS_LOG_DEBUG(session,msg) SESS_LOG(session,SWITCH_LOG_DEBUG, msg)
#define SESS_LOG_ERROR(session,msg) SESS_LOG(session,SWITCH_LOG_ERROR, msg)
#define SESS_LOG_NOTICE(session,msg) SESS_LOG(session,SWITCH_LOG_NOTICE, msg)
#define SESS_LOG_WARNING(session,msg) SESS_LOG(session,SWITCH_LOG_WARNING, msg)
#define SESS_LOG_CRIT(session,msg) SESS_LOG(session,SWITCH_LOG_CRIT, msg)

typedef boost::recursive_mutex mutex;
typedef boost::lock_guard<mutex> mutex_lock;
typedef boost::mutex mutex_critic_sec;
typedef boost::lock_guard<mutex_critic_sec> mutex_critic_sec_lock;
typedef boost::shared_mutex mutex_read_write;
typedef boost::shared_lock<boost::shared_mutex> mutex_read_lock;
typedef boost::lock_guard<boost::shared_mutex> mutex_write_lock;


template <typename T>
T sum_space_delimited_string_array(const std::string& str_array)
{
	std::vector<std::string> tokens = Telnyx::string_tokenize(str_array, " ");
	if (tokens.empty()) {
		return Telnyx::string_to_number<T>(str_array, 0);
	}
	T sum = 0;
	for (std::vector<std::string>::iterator iter = tokens.begin(); iter != tokens.end(); iter++) {
		sum += Telnyx::string_to_number<T>(*iter, 0);
	}
	return sum;
}

static inline std::string freeswitch_api_execute(const std::string& method, const std::string& args) {
	std::string result;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	switch_status_t status = switch_api_execute(method.c_str(), args.c_str(), NULL, &stream);

	if (stream.data) {
		result = std::string(static_cast<const char*>(stream.data));
	}
	switch_safe_free(stream.data);
	return result;
}

static inline std::string freeswitch_api_execute_raw(const std::string& command) {
	std::size_t method_pos = command.find(" ");
	if (method_pos == std::string::npos) {
		return freeswitch_api_execute(command, "");
	}
	std::string method = Telnyx::string_left(command, method_pos);
	std::string args = command.c_str() + method_pos + 1;
	return freeswitch_api_execute(method, args);
}
//
// Use this to get the number of open file handles
// WARNING!!! - This function is known to consume a lot of CPU
//
static inline int get_open_file_handles()
{
	int max_fd_number = getdtablesize();
	int fd_counter = 0;
	struct stat stats;
	for (int i = 0; i <= max_fd_number; i++ ) { 
		if (fcntl(i, F_GETFD) != -1) {
			fd_counter++; 
		} 
	}
	return fd_counter;
}

static inline void vector_to_carray(const std::vector<std::string>& args, char*** argv)
{
	*argv = (char**)std::malloc((args.size() + 1) * sizeof(char*));
	int i=0;
	for(std::vector<std::string>::const_iterator iter = args.begin();
		iter != args.end();
		iter++, ++i) {
		std::string arg = *iter;
		(*argv)[i] = (char*)std::malloc((arg.length()+1) * sizeof(char));
		std::strcpy((*argv)[i], arg.c_str());
	}
	(*argv)[args.size()] = NULL; // argv must be NULL terminated
}

static inline void free_carray(int argc, char*** argv)
{
  for (int i = 0; i < argc; i++)
    free((*argv)[i]);
  free(*argv);
}

static inline void system_abort(bool dump_cores)
{
	if (dump_cores) {
		switch_enable_dump_cores(true);
	}
	std::abort();
}

static inline bool read_text_file(const std::string& name, std::string& text) 
{
  std::ifstream textFile;
  textFile.open(name.c_str());
  
  if (!textFile.is_open() || !textFile.good())
  {
    return false;
  }
  std::ostringstream strm;
  
  while (!textFile.eof())
  {
    std::string line;
    std::getline(textFile, line);
    boost::trim(line);
    strm << line << std::endl;
  }
  
  text = strm.str();
  return true;
}

static inline bool get_channel_variable(switch_channel_t *channel, const char* name, std::string& value)
{
	if (!channel) {
		return false;
	}
	
	const char* val = switch_channel_get_variable_dup(channel, name, SWITCH_FALSE, -1);
	if (!val) {
		return false;
	}
	value = val;
	return true;
}

static inline bool set_channel_variable(switch_channel_t *channel, const char* name, const std::string& value)
{
	if (!channel) {
		return false;
	}
	switch_channel_set_variable(channel, name, value.c_str());
	return true;
}

static inline bool export_channel_variable_to_partner(switch_channel_t *channel, const char* name)
{
	switch_channel_t* other_channel = 0;
	std::string value;
	
	if (!get_channel_variable(channel, name, value)) {
		return false;
	}
	
	const char *uuid = switch_channel_get_partner_uuid(channel);
	if (uuid) {
		switch_core_session_t* other_session = 0;
		if ((other_session = switch_core_session_locate(uuid))) {
			other_channel = switch_core_session_get_channel(other_session);
			set_channel_variable(other_channel, name, value);
			switch_core_session_rwunlock(other_session);
		} else {
			return false;
		}
	} else {
		return false;
	}
	return true;
}


static inline bool get_partner_channel_variable(switch_channel_t *channel, const char* name, std::string& value)
{
	const char *uuid = switch_channel_get_partner_uuid(channel);
	switch_channel_t* other_channel = 0;
	bool ret = false;
	if (uuid) {
		switch_core_session_t* other_session = 0;
		if ((other_session = switch_core_session_locate(uuid))) {
			other_channel = switch_core_session_get_channel(other_session);
			if (other_channel) {
				ret = get_channel_variable(other_channel, name, value);
			}
			switch_core_session_rwunlock(other_session);
		} 
	} 
	return ret;
}

static inline std::string get_switch_ip4()
{
	char guess_ip4[256];
	switch_find_local_ip(guess_ip4, sizeof(guess_ip4), NULL, AF_INET);
	return guess_ip4;
}

static inline bool get_uri_head(const std::string& uri, std::string& head)
{
	std::size_t head_index = uri.find("sip:");
	std::size_t offset = 4;
	if (head_index == std::string::npos) {
		if ((head_index = uri.find("sips:")) == std::string::npos)
		{
			return false;
		}
		offset = 5;
	}
	head = uri.substr(0, head_index + offset);
	return true;
}

static inline bool split_uri(const std::string& uri, std::string& head, std::string& tail)
{
	if (!get_uri_head(uri, head)) {
		return false;
	}
	tail = uri.substr(head.length());
	return true;
}

static inline bool get_uri_user_from_tail(const std::string& tail, std::string& user)
{
	std::size_t index = tail.find("@");
	if (index == std::string::npos) {
		return false;
	}
	user = tail.substr(0, index);
	return true;
}

static inline bool get_uri_user(const std::string& uri, std::string& user)
{
	std::string head;
	std::string tail;

	if (!split_uri(uri, head, tail)) {
		return false;
	}
	return get_uri_user_from_tail(tail, user);
}

static inline bool get_uri_user_host_port_from_tail(const std::string& tail, std::string& user, std::string& host_port)
{
	get_uri_user_from_tail(tail, user);
	std::string newTail(tail);
	if (!user.empty()) {
		newTail = tail.substr(user.length() + 1);
	}
	for (std::string::iterator iter = newTail.begin(); iter != newTail.end(); iter++)
	{
		if (*iter != ';' && *iter != '>' ) {
			host_port.push_back(*iter);
		} else {
			break;
		}
	}
	return !host_port.empty();
}

static inline bool get_uri_user_host_port(const std::string& uri, std::string& user, std::string& host_port)
{
	std::string head;
	std::string tail;
	user = "";
	host_port = "";

	if (!split_uri(uri, head, tail)) {
		return false;
	}
	return get_uri_user_host_port_from_tail(tail, user, host_port);
}

static inline bool change_uri_host_port(std::string& uri, const std::string& new_host_port)
{
	std::string head;
	std::string tail;
	std::string user;
	std::string host_port;

	if (!split_uri(uri, head, tail) || !get_uri_user_host_port_from_tail(tail, user, host_port)) {
		return false;
	}

	std::ostringstream strm;
	if (!user.empty()) {
		strm << head << user << "@" << new_host_port << tail.substr(user.length() + host_port.length() + 1);
	} else {
		strm << head << new_host_port << tail.substr(host_port.length());
	}
	uri = strm.str();
	return true;
}

#endif /* MOD_UTILS_H */

