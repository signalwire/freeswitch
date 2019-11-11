// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#include <boost/thread.hpp>
#include "Poco/AutoPtr.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/SplitterChannel.h"
#include "Poco/FileChannel.h"
#include "Poco/PatternFormatter.h"
#include "Poco/FormattingChannel.h"
#include "Poco/Message.h"
#include "Poco/LogFile.h"
#include "Poco/Timestamp.h"
#include "Poco/LogStream.h"
#include <iostream>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/operations.hpp>

#include "Telnyx/UTL/LogFile.h"

namespace Telnyx {
namespace UTL {
 

/*
   
  The format pattern is used as a template to format the message and is copied character by character except for the following special characters, which are replaced by the corresponding value.

  %s - message source
  %t - message text
  %l - message priority level (1 .. 7)
  %p - message priority (Fatal, Critical, Error, Warning, Notice, Information, Debug, Trace)
  %q - abbreviated message priority (F, C, E, W, N, I, D, T)
  %P - message process identifier
  %T - message thread name
  %I - message thread identifier (numeric)
  %N - node or host name
  %U - message source file path (empty string if not set)
  %u - message source line number (0 if not set)
  %w - message date/time abbreviated weekday (Mon, Tue, ...)
  %W - message date/time full weekday (Monday, Tuesday, ...)
  %b - message date/time abbreviated month (Jan, Feb, ...)
  %B - message date/time full month (January, February, ...)
  %d - message date/time zero-padded day of month (01 .. 31)
  %e - message date/time day of month (1 .. 31)
  %f - message date/time space-padded day of month ( 1 .. 31)
  %m - message date/time zero-padded month (01 .. 12)
  %n - message date/time month (1 .. 12)
  %o - message date/time space-padded month ( 1 .. 12)
  %y - message date/time year without century (70)
  %Y - message date/time year with century (1970)
  %H - message date/time hour (00 .. 23)
  %h - message date/time hour (00 .. 12)
  %a - message date/time am/pm
  %A - message date/time AM/PM
  %M - message date/time minute (00 .. 59)
  %S - message date/time second (00 .. 59)
  %i - message date/time millisecond (000 .. 999)
  %c - message date/time centisecond (0 .. 9)
  %F - message date/time fractional seconds/microseconds (000000 - 999999)
  %z - time zone differential in ISO 8601 format (Z or +NN.NN)
  %Z - time zone differential in RFC format (GMT or +NNNN)
  %L - convert time to local time (must be specified before any date/time specifier; does not itself output anything)
  %E - epoch time (UTC, seconds since midnight, January 1, 1970)
  %v[width] - the message source (%s) but text length is padded/cropped to 'width'
  %[name] - the value of the message parameter with the given name
  %% - percent sign
 
*/

  static const std::string LOGGER_DEFAULT_NAME = "Core Logger";
  static const std::string LOGGER_DEFAULT_FORMAT = "%h-%M-%S.%i: %t"; 
  static const LogFile::Priority LOGGER_DEFAULT_PRIORITY = LogFile::PRIO_INFORMATION;
  static const unsigned int LOGGER_DEFAULT_PURGE_COUNT = 7; /// Default to one week
  static unsigned int DEFAULT_VERIFY_TTL = 5; /// TTL in seconds for verification to kick in

    
  static Poco::Message::Priority poco_priority(Telnyx::UTL::LogFile::Priority priority)
  {
    switch (priority)
    {
      case Telnyx::UTL::LogFile::PRIO_FATAL:
        return Poco::Message::PRIO_FATAL;
      case Telnyx::UTL::LogFile::PRIO_CRITICAL:
        return Poco::Message::PRIO_CRITICAL;
      case Telnyx::UTL::LogFile::PRIO_ERROR:
        return Poco::Message::PRIO_ERROR;
      case Telnyx::UTL::LogFile::PRIO_WARNING:
        return Poco::Message::PRIO_WARNING;
      case Telnyx::UTL::LogFile::PRIO_NOTICE:
        return Poco::Message::PRIO_NOTICE;
      case Telnyx::UTL::LogFile::PRIO_INFORMATION:
        return Poco::Message::PRIO_INFORMATION;
      case Telnyx::UTL::LogFile::PRIO_DEBUG:
        return Poco::Message::PRIO_DEBUG;
      case Telnyx::UTL::LogFile::PRIO_TRACE:
        return Poco::Message::PRIO_TRACE;
    }
    
    //
    // Someone is too drunk to code.  Should never get here.
    //
    assert(false);
  }
    
  LogFile::LogFile(const std::string& name) :
    _name(name),
    _instanceCount(0),
    _lastVerifyTime(0),
    _enableVerification(true),
    _verificationInterval(DEFAULT_VERIFY_TTL),
    _isOpen(false)
  {
    std::ostringstream strm;
    strm << _name << "-" << _instanceCount;
    _internalName = strm.str();
  }

  LogFile::~LogFile()
  {
    //
    // Grab the mutex before calling close to make sure we do not corrupt
    // any pointers within the current executing log message
    //
    mutex_lock lock(_mutex);
    close();
  }


  bool LogFile::open(
    const std::string& path 
  )
  {
    return open(path, LOGGER_DEFAULT_PRIORITY);
  }
  
  bool LogFile::open(
    const std::string& path,
    Priority priority
  )
  {
    return open(path, priority, LOGGER_DEFAULT_FORMAT);
  }

  bool LogFile::open(
    const std::string& path,  
    Priority priority,
    const std::string& format 
  )
  {
    return open(path, priority, format, LOGGER_DEFAULT_PURGE_COUNT);
  }

  bool LogFile::open(
    const std::string& path,  
    Priority priority,
    const std::string& format, 
    unsigned int purgeCount 
  )
  {
    if (_isOpen)
    {
      warning("LogFile::open invoked while already in open state.  Close the logger first by calling LogFile::close()");
      return false;
    }
    
    try
    {
      _path = path;
      _priority = priority;
      _format = format;
      _purgeCount = purgeCount;
   
      Poco::AutoPtr<Poco::FileChannel> fileChannel(new Poco::FileChannel(path));

      if (_purgeCount > 0)
      {
        std::string strPurgeCount = boost::lexical_cast<std::string>(purgeCount);
        fileChannel->setProperty("rotation", "daily");
        fileChannel->setProperty("archive", "timestamp");
        fileChannel->setProperty("compress", "true");
        fileChannel->setProperty("purgeCount", strPurgeCount);
      }

      //
      // increment the instance name so that we use a 
      // new logger instance when we open/reopen the channel
      //
      std::ostringstream strmName;
      strmName << _name << "-" << ++_instanceCount;
      _internalName = strmName.str();
      
      Poco::AutoPtr<Poco::Formatter> formatter(new Poco::PatternFormatter(format.c_str()));
      Poco::AutoPtr<Poco::Channel> formattingChannel(new Poco::FormattingChannel(formatter, fileChannel));
      Poco::Logger::create(_internalName, formattingChannel, poco_priority(priority));
      
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {          
        _lastError = "";
        _isOpen = true;
      }
      else
      {
        _lastError = "LogFile::open - Poco::Logger is null";
        _isOpen = false;
      }
    }
    catch(const std::exception& e)
    {
      _lastError = "LogFile::open - ";
      _lastError += e.what();
      close();
      _isOpen = false;
    }
    catch(...)
    {
      _lastError = "LogFile::open unknown exception";
      close();
      _isOpen = false;
    }
    
    return _isOpen;
  }

  void LogFile::close()
  {
    if (Poco::Logger::has(_internalName))
      Poco::Logger::destroy(_internalName);
    
    _isOpen = Poco::Logger::has(_internalName);
  }
 
  void LogFile::setPriority(LogFile::Priority priority)
  {
    _priority = priority;
    Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
    if (pLogFile)
    {
      pLogFile->setLevel(poco_priority(priority));
    }
  }
  
  bool LogFile::willLog(Priority priority) const
  {
    return priority <= _priority;
  }

  void LogFile::fatal(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_FATAL) && (_enableVerification ? verifyLogFile(false) : isOpen()) )
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->fatal(log);
      }
    }
  }

  void LogFile::critical(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_CRITICAL) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->critical(log);
      }
    }
  }

  void LogFile::error(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_ERROR) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->error(log);
      }
    }
  }

  void LogFile::warning(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_WARNING) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->warning(log);
      }
    }
  }

  void LogFile::notice(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_NOTICE) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->notice(log);
      }
    }
  }

  void LogFile::information(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_INFORMATION) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->information(log);
      }
    }
  }

  void LogFile::debug(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_DEBUG) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->debug(log);
      }
    }
  }

  void LogFile::trace(const std::string& log)
  {
    //
    // We need to make this thread safe or calls from 
    // different thread might try to reopen the logger
    // at the same time when calling verifyLogFile.
    // This can result to a segmentation fault if pLogFile
    // is released from another thread
    //
    mutex_lock lock(_mutex);
    
    if (willLog(PRIO_TRACE) && (_enableVerification ? verifyLogFile(false) : isOpen()))
    {
      Poco::Logger* pLogFile = Poco::Logger::has(_internalName);
      if (pLogFile)
      {
        pLogFile->trace(log);
      }
    }
  }
  
  bool LogFile::verifyLogFile(bool force)
  {    
    if (!_isOpen)
    {
      //
      // log file cannot be verified because logger is not open
      //
      return false;
    }
    
    Poco::Timestamp timeStamp;
    std::time_t now = timeStamp.epochTime();
    
    if (!force && (now - _lastVerifyTime < _verificationInterval))
    {
      return true; // TTL for this file has not yet expired
    }
    
    _lastVerifyTime = now;
    
    if (!_path.empty() && !boost::filesystem::exists(_path))
    {
      //
      // Close the old logger.  We are about to reopen a new one
      //
      close();
      
      return open(_path, _priority, _format, _purgeCount); 
    }

    return _isOpen;
  }

} } // Telnyx::UTL