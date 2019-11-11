// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/Telnyx.h"

#include "Poco/AutoPtr.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/SplitterChannel.h"
#include "Poco/FileChannel.h"
#include "Poco/PatternFormatter.h"
#include "Poco/FormattingChannel.h"
#include "Poco/Message.h"
#include "Poco/Logger.h"
#include <iostream>
#include <sstream>

#include "Telnyx/UTL/Logger.h"
#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/Thread.h"


using Poco::AutoPtr;
using Poco::Channel;
using Poco::ConsoleChannel;
using Poco::SplitterChannel;
using Poco::FileChannel;
using Poco::FormattingChannel;
using Poco::Formatter;
using Poco::PatternFormatter;
using Poco::Logger;
using Poco::Message;


namespace Telnyx {


static Poco::Logger* _pLogger = 0;
static boost::filesystem::path _logFile;
static boost::filesystem::path _logDirectory;;
static Telnyx::mutex_critic_sec _consoleMutex;
static bool _enableConsoleLogging = true;
static bool _enableLogging = true;
static LogPriority _consoleLogLevel = PRIO_INFORMATION;
static ExternalLogger _externalLogger;
  /*
enum LogPriority
{
	PRIO_FATAL = 1,   /// A fatal error. The application will most likely terminate. This is the highest priority.
	PRIO_CRITICAL,    /// A critical error. The application might not be able to continue running successfully.
	PRIO_ERROR,       /// An error. An operation did not complete successfully, but the application as a whole is not affected.
	PRIO_WARNING,     /// A warning. An operation completed with an unexpected result.
	PRIO_NOTICE,      /// A notice, which is an information with just a higher priority.
	PRIO_INFORMATION, /// An informational message, usually denoting the successful completion of an operation.
	PRIO_DEBUG,       /// A debugging message.
	PRIO_TRACE        /// A tracing message. This is the lowest priority.
};
*/

/// This Formatter allows for custom formatting of
/// log messages based on format patterns.
///
/// The format pattern is used as a template to format the message and
/// is copied character by character except for the following special characters,
/// which are replaced by the corresponding value.
///
///   * %s - message source
///   * %t - message text
///   * %l - message priority level (1 .. 7)
///   * %p - message priority (Fatal, Critical, Error, Warning, Notice, Information, Debug, Trace)
///   * %q - abbreviated message priority (F, C, E, W, N, I, D, T)
///   * %P - message process identifier
///   * %T - message thread name
///   * %I - message thread identifier (numeric)
///   * %N - node or host name
///   * %U - message source file path (empty string if not set)
///   * %u - message source line number (0 if not set)
///   * %w - message date/time abbreviated weekday (Mon, Tue, ...)
///   * %W - message date/time full weekday (Monday, Tuesday, ...)
///   * %b - message date/time abbreviated month (Jan, Feb, ...)
///   * %B - message date/time full month (January, February, ...)
///   * %d - message date/time zero-padded day of month (01 .. 31)
///   * %e - message date/time day of month (1 .. 31)
///   * %f - message date/time space-padded day of month ( 1 .. 31)
///   * %m - message date/time zero-padded month (01 .. 12)
///   * %n - message date/time month (1 .. 12)
///   * %o - message date/time space-padded month ( 1 .. 12)
///   * %y - message date/time year without century (70)
///   * %Y - message date/time year with century (1970)
///   * %H - message date/time hour (00 .. 23)
///   * %h - message date/time hour (00 .. 12)
///   * %a - message date/time am/pm
///   * %A - message date/time AM/PM
///   * %M - message date/time minute (00 .. 59)
///   * %S - message date/time second (00 .. 59)
///   * %i - message date/time millisecond (000 .. 999)
///   * %c - message date/time centisecond (0 .. 9)
///   * %F - message date/time fractional seconds/microseconds (000000 - 999999)
///   * %z - time zone differential in ISO 8601 format (Z or +NN.NN)
///   * %Z - time zone differential in RFC format (GMT or +NNNN)
///   * %L - convert time to local time (must be specified before any date/time specifier; does not itself output anything)
///   * %E - epoch time (UTC, seconds since midnight, January 1, 1970)
///   * %v[width] - the message source (%s) but text length is padded/cropped to 'width'
///   * %[name] - the value of the message parameter with the given name
///   * %% - percent sign

void log_enable_console(bool yes)
{
  _enableConsoleLogging = yes;
}

void log_enable_logging(bool yes)
{
  _enableLogging = yes;
}

void logger_init_external(const ExternalLogger& externalLogger)
{
  _consoleMutex.lock();
  _externalLogger = externalLogger;
  _consoleMutex.unlock();
}

void logger_init(
  const std::string& path,
  LogPriority level,
  const std::string& format,
  const std::string& compress,
  const std::string& purgeCount,
  const std::string& times)
{
  if (_enableLogging)
  {
    _logFile = path;

    if (!_logDirectory.empty())
    {
      //
      // Application override for the directory
      //
      std::string lfile = Telnyx::boost_file_name(_logFile);
      _logFile = operator/(_logDirectory, lfile);
    }

    AutoPtr<FileChannel> rotatedFileChannel(new FileChannel(Telnyx::boost_path(_logFile)));
    rotatedFileChannel->setProperty("rotation", "daily");
    rotatedFileChannel->setProperty("archive", "timestamp");
    rotatedFileChannel->setProperty("compress", compress);
    rotatedFileChannel->setProperty("purgeCount", purgeCount);

    AutoPtr<Formatter> formatter(new PatternFormatter(format.c_str()));
    AutoPtr<Channel> formattingChannel(new FormattingChannel(formatter, rotatedFileChannel));
    formatter->setProperty("times", times);
    _pLogger = &(Logger::create("OSS.logger", formattingChannel, level));
  }
}

void logger_deinit()
{
  if (_pLogger)
    _pLogger->destroy("OSS.logger");
}

void log_reset_level(LogPriority level)
{
  if (!_enableLogging)
    return;

  if (_pLogger)
    _pLogger->setLevel(level);

  _consoleLogLevel = level;
}

LogPriority log_get_level()
{
  if (!_enableLogging || !_pLogger)
    return _consoleLogLevel;

  return static_cast<LogPriority>(_pLogger->getLevel());
}

const boost::filesystem::path& logger_get_path()
{
  return _logFile;
}

void logger_set_directory(const boost::filesystem::path& directory)
{
  _logDirectory = directory;
}

void log(const std::string& log, LogPriority priority)
{
  if (!_enableLogging)
    return;

  switch (priority)
  {
  case PRIO_FATAL :
    log_fatal(log);
    break;
  case PRIO_CRITICAL :
    log_critical(log);
    break;
  case PRIO_ERROR :
    log_error(log);
    break;
  case PRIO_WARNING :
    log_warning(log);
    break;
  case PRIO_NOTICE :
    log_notice(log);
    break;
  case PRIO_INFORMATION :
    log_information(log);
    break;
  case PRIO_DEBUG :
    log_debug(log);
    break;
  case PRIO_TRACE :
    log_trace(log);
    break;
  default:
    TELNYX_VERIFY(false);
  }
}

void log_fatal(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;
  
  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_FATAL);
    }
    _consoleMutex.unlock();
    return;
  }

  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_FATAL)
  {
    _consoleMutex.lock();
    std::cout << "[FATAL] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }
  
  if (_pLogger)
    _pLogger->fatal(log);
}

void log_critical(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;
  
  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_CRITICAL);
    }
    _consoleMutex.unlock();
    return;
  }

  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_CRITICAL)
  {
    _consoleMutex.lock();
    std::cout << "[CRITICAL] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }

  if (_pLogger)
    _pLogger->critical(log);
}

void log_error(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;

  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_ERROR);
    }
    _consoleMutex.unlock();
    return;
  }
  
  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_ERROR)
  {
    _consoleMutex.lock();
    std::cout << "[ERROR] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }
  
  if (_pLogger)
    _pLogger->error(log);
}

void log_warning(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;

  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_WARNING);
    }
    _consoleMutex.unlock();
    return;
  }
  
  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_WARNING)
  {
    _consoleMutex.lock();
    std::cout << "[WARNING] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }
  
  if (_pLogger)
    _pLogger->warning(log);
}

void log_notice(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;

  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_NOTICE);
    }
    _consoleMutex.unlock();
    return;
  }
  
  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_NOTICE)
  {
    _consoleMutex.lock();
    std::cout << "[NOTICE] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }

  if (_pLogger)
    _pLogger->notice(log);
}

void log_information(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;
  
  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_INFORMATION);
    }
    _consoleMutex.unlock();
    return;
  }

  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_INFORMATION)
  {
    _consoleMutex.lock();
    std::cout << "[INFORMATION] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }
  
  if (_pLogger)
    _pLogger->information(log);
}

void log_debug(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;

  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_DEBUG);
    }
    _consoleMutex.unlock();
    return;
  }
  
  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_DEBUG)
  {
    _consoleMutex.lock();
    std::cout << "[DEBUG] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }
  
  if (_pLogger)
    _pLogger->debug(log);
}

void log_trace(const std::string& log)
{
  if (!_enableLogging || log.empty())
    return;

  if (_externalLogger)
  {
    // the inner check is guaranteed becasue of the mutex
    _consoleMutex.lock();
    if (_externalLogger)
    {
	_externalLogger(log, PRIO_TRACE);
    }
    _consoleMutex.unlock();
    return;
  }
  
  if(!_pLogger && _enableConsoleLogging && _consoleLogLevel >= PRIO_TRACE)
  {
    _consoleMutex.lock();
    std::cout << "[TRACE] " << log << std::endl;
    _consoleMutex.unlock();
    return;
  }
  
  if (_pLogger)
    _pLogger->trace(log);
}

} // OSS

