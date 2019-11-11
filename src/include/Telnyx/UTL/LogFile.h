// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef LOGFILE_H_INCLUDED
#define	LOGFILE_H_INCLUDED


#include <ctime>
#include <string>
#include <sstream>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>


namespace Telnyx {
namespace UTL {
  
  
class LogFile : public boost::noncopyable
  {
  public:
    
    typedef boost::mutex mutex;
    typedef boost::lock_guard<mutex> mutex_lock;
    
    enum Priority
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

    LogFile(const std::string& name);
    ///
    /// Creates a new logger
    ///
    
    ~LogFile();
    ///
    /// Destroy the logger.  Close channel if open
    ///
    
    
    
    bool open(
      const std::string& path // path for the log file
    );
    ///
    /// Open the log file specified by path
    /// If error is encountered, getLastError()should return the error string
    ///
    
    bool open(
      const std::string& path,  // path for the log file
      Priority priority // Log priority level
    );
    ///
    /// Open the log file specified by path
    /// If error is encountered, getLastError()should return the error string
    ///
    
    bool open(
      const std::string& path,  // path for the log file
      Priority priority, // Log priority level
      const std::string& format // format for the log headers
    );
    ///
    /// Open the log file specified by path
    /// If error is encountered, getLastError()should return the error string
    ///
    
    bool open(
      const std::string& path,  // path for the log file 
      Priority priority, // Log priority level
      const std::string& format, // format for the log headers
      unsigned int purgeCount // number of files to maintain during log rotation
    );
    ///
    /// Open the log file specified by path
    /// If error is encountered, getLastError()should return the error string
    ///
    
    const std::string& getName() const;
    ///
    /// Returns the logger name specified in constructor
    ///
       
    const std::string& getLogFormat()const;
    ///
    /// Returns the log format specified in open.  Default: %h-%M-%S.%i: %t
    ///
    
    const std::string& getPath() const;
    ///
    /// Returns the path of the log file specified in open()
    ///
    
    unsigned int getPurgeCount() const;
    ///
    /// Returns the purge count specified in open())
    ///
    
    Priority getPriority() const;
    ///
    /// Returns the current priority level
    ///
    
    void setPriority(Priority priority);
    ///
    /// Set the priority level of the logger
    ///
    
    void fatal(const std::string& log);
    ///
    /// Log a message in fatal level 
    ///
    
    void critical(const std::string& log);
    ///
    /// Log a message in critical level 
    ///
    
    void error(const std::string& log);
    ///
    /// Log a message in error level 
    ///
    
    void warning(const std::string& log);
    ///
    /// Log a message in warning level 
    ///
    
    void notice(const std::string& log);
    ///
    /// Log a message in notice level 
    ///
    
    void information(const std::string& log);
    ///
    /// Log a message in info level 
    ///
    
    void debug(const std::string& log);
    ///
    /// Log a message in debug level 
    ///
    
    void trace(const std::string& log);
    ///
    /// Log a message in trace level 
    ///
    
    bool willLog(Priority priority) const;
    ///
    /// Return true if priority is >= _priority
    ///
    
    static LogFile* instance();
    ///
    /// Returns the default logger instance.
    ///
    
    static void releaseInstance();
    ///
    /// Delete the default logger instance
    ///
    
    bool isOpen() const;
    ///
    /// Returns true if the logger is open and is in ready state 
    ///
    
    const std::string& getLastError() const;
    ///
    /// Returns the error string after a failure to open the logger
    ///
    
    void enableVerification(bool enable);
    ///
    /// Enable/Disable log file verification.  If disabled log file
    /// will not be reopened if deleted from the disk.
    /// Default:  true
    ///
    
    void setVerificationInterval(unsigned int seconds);
    ///
    /// Set the verification interval.  This is expressed un seconds.
    /// Default:  5 seconds
    ///
    
  protected:
    void close();
    ///
    /// Close the logging channel.  This is not a thread safe call
    /// and is intended to be called within the logger internals only.
    /// If applications need to close a logger, it must simply delete
    /// the old instance and create a new instance.
    ///
    
    bool verifyLogFile(bool force);
    ///
    /// Ensure that the log file exists.
    /// if not, reopen it
    /// if force is true, verification will take place irregardless of the
    /// verification interval.  This is not a thread safe call
    /// and is intended to be called within the logger internals only.
    
  private:
    static LogFile* _pLogFileInstance; /// Pointer to the default logger instance
    std::string _name; /// The logger name 
    std::string _format; /// Log format string
    std::string _path; /// Path of the log file
    bool _enableCompression; /// Flag to enable compression during rotation
    unsigned int _purgeCount; /// Number of log files to be maintained after last rotation
    Priority _priority; /// The log priority level
    unsigned int _instanceCount;  /// USed to reconstruct a new name for the logger
    std::string _internalName;  /// The internal name of this logger
    std::time_t _lastVerifyTime;  /// The time the last verification was done
    bool _enableVerification; /// enable/disable verification
    unsigned int _verificationInterval; /// Expiration for verification expressed in seconds
    bool _isOpen;  /// Flag indicator if logger is open
    std::string _lastError;  /// last error encountered after a logger function is invoked
    mutex _mutex;  /// Internal mutex
  };
  
  //
  // Inlines
  //
  
  inline const std::string& LogFile::getName() const
  {
    return _name;
  }

  inline const std::string& LogFile::getLogFormat() const
  {
    return _format;
  }

  inline const std::string& LogFile::getPath() const
  {
    return _path;
  }

  inline unsigned int LogFile::getPurgeCount() const
  {
    return _purgeCount;
  }
  
  inline LogFile::Priority LogFile::getPriority() const
  {
    return _priority;
  }
  
  inline bool LogFile::isOpen() const
  {
    return _isOpen;
  }
  
  inline const std::string& LogFile::getLastError() const
  {
    return _lastError;
  }
  
  inline void LogFile::enableVerification(bool enable)
  {
    _enableVerification = enable;
  }
  
  inline void LogFile::setVerificationInterval(unsigned int seconds)
  {
    _verificationInterval = seconds;
  }
  
  
} } // Telnyx::UTL

#endif	// LOGFILE_H_INCLUDED

