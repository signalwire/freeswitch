// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_LOGGER_H_INCLUDED
#define TELNYX_LOGGER_H_INCLUDED

#include <iostream>
#include <sstream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>

#include "Telnyx/Telnyx.h"


namespace Telnyx {

enum LogPriority
{
  PRIO_NONE,
	PRIO_FATAL = 1,   /// A fatal error. The application will most likely terminate. This is the highest priority.
	PRIO_CRITICAL,    /// A critical error. The application might not be able to continue running successfully.
	PRIO_ERROR,       /// An error. An operation did not complete successfully, but the application as a whole is not affected.
	PRIO_WARNING,     /// A warning. An operation completed with an unexpected result.
	PRIO_NOTICE,      /// A notice, which is an information with just a higher priority.
	PRIO_INFORMATION, /// An informational message, usually denoting the successful completion of an operation.
	PRIO_DEBUG,       /// A debugging message.
	PRIO_TRACE        /// A tracing message. This is the lowest priority.
};

typedef boost::function<void(const std::string&, LogPriority)> ExternalLogger;

void TELNYX_API logger_init(
  const std::string& path,
  LogPriority level = Telnyx::PRIO_INFORMATION,
  const std::string& format = "%Y-%m-%d %H:%M:%S %s: [%p] %t",
  const std::string& compress = "true",
  const std::string& purgeCount = "7",
  const std::string& times = "UTC");
  /// Initialize the logging subsystem from the config specified

void logger_init_external(const ExternalLogger& externalLogger);

void TELNYX_API logger_deinit();
  /// Deinitialize the logger

void TELNYX_API log_reset_level(LogPriority level);
  /// Reset the the log level

LogPriority TELNYX_API log_get_level();
  /// Return the the log level

const boost::filesystem::path& TELNYX_API logger_get_path();

void TELNYX_API logger_set_directory(const boost::filesystem::path& directory);

void TELNYX_API log(const std::string& log, LogPriority priority = Telnyx::PRIO_INFORMATION);
  /// Log a message with prioty specified

void TELNYX_API log_fatal(const std::string& log);
  /// Log a fatal error. The application will most likely terminate. This is the highest priority.

#define TELNYX_LOG_FATAL(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_fatal(strm.str()); \
}

void TELNYX_API log_critical(const std::string& log);
  /// Log a critical error. The application might not be able to continue running successfully.

#define TELNYX_LOG_CRITICAL(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_critical(strm.str()); \
}

void TELNYX_API log_error(const std::string& log);
  /// Log an error. An operation did not complete successfully, but the application as a whole is not affected.

void TELNYX_API log_enable_console(bool yes = true);
  /// Enable console logging if file output is not specified

void TELNYX_API log_enable_logging(bool yes);
  /// Enabel or disable logging.  If set to false, no log output will be written to either console or file


#define TELNYX_LOG_ERROR(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_error(strm.str()); \
}

void TELNYX_API log_warning(const std::string& log);
  /// Log a warning. An operation completed with an unexpected result.

#define TELNYX_LOG_WARNING(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_warning(strm.str()); \
}

void TELNYX_API log_notice(const std::string& log);
  /// Log a notice, which is an information with just a higher priority.

#define TELNYX_LOG_NOTICE(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_notice(strm.str()); \
}

void TELNYX_API log_information(const std::string& log);
  /// Log an informational message, usually denoting the successful completion of an operation.

#define TELNYX_LOG_INFO(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_information(strm.str()); \
}


void TELNYX_API log_debug(const std::string& log);
  /// Log a debugging message.

#define TELNYX_LOG_DEBUG(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_debug(strm.str()); \
}

void TELNYX_API log_trace(const std::string& log);
  /// Log a tracing message. This is the lowest priority.

#define TELNYX_LOG_TRACE(log) \
{ \
  std::ostringstream strm; \
  strm << log; \
  Telnyx::log_trace(strm.str()); \
}

#ifdef _DEBUG
#define TELNYX_LOG_DEV_TRACE TELNYX_LOG_NOTICE
#else
#define TELNYX_LOG_DEV_TRACE
#endif


} // OSS
#endif // TELNYX_LOGGER_H_INCLUDED
