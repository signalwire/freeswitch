// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_SERVICEOPTIONS_H_INCLUDED
#define	TELNYX_SERVICEOPTIONS_H_INCLUDED

#include <cstdlib>
#include <cassert>
#include <csignal>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/detail/ptree_utils.hpp>
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>
#include <locale>

#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/Logger.h"



namespace Telnyx {


class ServiceOptions
{
public:
  enum OptionType
  {
    GeneralOption,
    DaemonOption,
    ConfigOption
  };
  
  enum ProcType
  {
    Daemon,
    Utility,
    MaxProcType
  };
  
  ServiceOptions(int argc, char** argv, const std::string& daemonName, const std::string& version = "1.0", const std::string& copyright = "All Rights Reserved.", ProcType procType = Daemon);
  ServiceOptions(int argc, char** argv, const char* daemonName, const char* version = "1.0", const char* copyright = "All Rights Reserved.", ProcType procType = Daemon);
  ServiceOptions(const std::string& configFile);
  virtual ~ServiceOptions();
  //
  // Options processing
  //
  void addOptionFlag(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionFlag(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionString(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionString(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionStringVector(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionStringVector(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionInt(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionInt(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionIntVector(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addOptionIntVector(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  //
  // Mandatory flags
  //
  void addRequiredString(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredString(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredInt(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredInt(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredStringVector(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredStringVector(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredIntVector(char shortForm, const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  void addRequiredIntVector(const std::string& optionName, const std::string description, OptionType type = ConfigOption);
  //
  // Standard daemon options
  //
  void addDaemonOptions();
  static void  daemonize(int argc, char** argv, bool isDaemon);
  static void catch_global();

  bool parseOptions(bool verbose = false);
  void prepareLogger();
  void displayUsage(std::ostream& strm) const;
  void displayVersion(std::ostream& strm) const;
  size_t hasOption(const std::string& optionName, bool consolidate = true) const;
  size_t hasConfigOption(const std::string& optionName) const;
  bool getOption(const std::string& optionName, std::string& value, const std::string& defValue = std::string()) const;
  bool getOption(const std::string& optionName, std::vector<std::string>& value) const;
  bool getOption(const std::string& optionName, int& value) const;
  bool getOption(const std::string& optionName, int& value, int defValue) const;
  bool getOption(const std::string& optionName, std::vector<int>& value) const;
  bool getOption(const std::string& optionName, bool& value, bool defValue) const;
  
  void setLogFormat(const std::string& logFormat);
  const std::string& getLogFormat() const;
  
  void setLogTimes(const std::string& logTimes);
  const std::string& getLogTimes() const;
  
  void setLogPurgeCount(int purgeCount);
  int getLogPurgeCount();

  virtual bool onParseUnknownOptions(int argc, char** argv) {return false;};
  void waitForTerminationRequest();

  pid_t  writePidFile(const char* pidFile, bool exclusive);
  
  int argc();
  char** argv();
protected:
  boost::program_options::options_description* options_description(OptionType type);
  
  int _argc;
  char** _argv;
  std::string _daemonName;
  std::string _version;
  std::string _copyright;

  boost::program_options::options_description _daemonOptions;
  boost::program_options::options_description _GeneralOptions;
  boost::program_options::options_description _configOptions;
  boost::program_options::options_description _optionItems;
  boost::program_options::variables_map _options;
  //
  // Pre-parsed values
  //
  bool _isDaemon;
  std::string _pidFile;
  std::string _configFile;
  boost::property_tree::ptree _ptree;
  bool _hasConfig;
  bool _isConfigOnly;
  std::vector<std::string> _required;
  ProcType _procType;
  std::string _logFormat;
  std::string _logTimes;
  int _logPurgeCount;
};

inline ServiceOptions::ServiceOptions(int argc, char** argv,
  const std::string& daemonName,
  const std::string& version,
  const std::string& copyright,
  ProcType procType) :
  _argc(argc),
  _argv(argv),
  _daemonName(daemonName),
  _version(version),
  _copyright(copyright),
  _daemonOptions("Daemon"),
  _GeneralOptions("General"),
  _configOptions("Configuration"),
  _optionItems(_daemonName  + " Options"),
  _isDaemon(false),
  _hasConfig(false),
  _isConfigOnly(false),
  _procType(procType),
  _logPurgeCount(7)
{
}

inline ServiceOptions::ServiceOptions(int argc, char** argv,
  const char* daemonName,
  const char* version,
  const char* copyright,
  ProcType procType) :
  _argc(argc),
  _argv(argv),
  _daemonName(daemonName),
  _version(version),
  _copyright(copyright),
  _daemonOptions("Daemon"),
  _GeneralOptions("General"),
  _configOptions("Configuration"),
  _optionItems(_daemonName  + " Options"),
  _isDaemon(false),
  _hasConfig(false),
  _isConfigOnly(false),
  _procType(procType),
  _logPurgeCount(7)
{
}

inline ServiceOptions::ServiceOptions(const std::string& configFile) :
  _argc(0),
  _argv(0),
  _daemonOptions("Daemon"),
  _GeneralOptions("General"),
  _configOptions("Configuration"),
  _optionItems(_daemonName  + " Options"),
  _isDaemon(false),
  _configFile(configFile),
  _hasConfig(true),
  _isConfigOnly(true),
  _procType(MaxProcType)
{
}

//
// inlines
//

inline void ServiceOptions::addDaemonOptions()
{
  if (_procType == Daemon)
  {
    addOptionFlag('D', "daemonize", ": Run as system daemon.", DaemonOption);
    addOptionString('P', "pid-file", ": PID file when running as daemon.", DaemonOption);
  }
}


inline bool ServiceOptions::parseOptions(bool verbose)
{

  if (_isConfigOnly)
  {
    try
    {
      boost::property_tree::ini_parser::read_ini(_configFile.c_str(), _ptree);
      _hasConfig = true;
    }
    catch(const std::exception& e)
    {
      if(verbose)
        std::cerr << _daemonName << " is not able to parse the options - " << e.what() << std::endl;
      return false;
    }
    return true;
  }

  try
  {
    addOptionFlag('h', "help", ": Display help information.", GeneralOption);
    addOptionFlag('v', "version", ": Display version information.", GeneralOption);
    
    if (_procType == Daemon)
    {
      addOptionString('C', "config-file", ": Optional daemon config file.", GeneralOption);


      addOptionString('L', "log-file", ": Specify the application log file.", GeneralOption);
      addOptionInt('l', "log-level",
        ": Specify the application log priority level."
        "Valid level is between 0-7.  "
        "0 (EMERG) 1 (ALERT) 2 (CRIT) 3 (ERR) 4 (WARNING) 5 (NOTICE) 6 (INFO) 7 (DEBUG)"
              , GeneralOption);
      addOptionFlag("log-no-compress", ": Specify if logs will be compressed after rotation.", GeneralOption);
      addOptionInt("log-purge-count", ": Specify the number of archive to maintain.", GeneralOption);
      addOptionString("log-pattern", ": Specify the pattern of the log headers. Default is \"%Y-%m-%d %H:%M:%S %s: [%p] %t\"", GeneralOption);
      addOptionString("log-times", ": Specifies whether times are adjusted for local time or taken as they are in UTC. Supported values are \"local\" and \"UTC\"", GeneralOption);
    }

    _optionItems.add(_GeneralOptions);
    
    if (_procType == Daemon)
      _optionItems.add(_daemonOptions);
    
    _optionItems.add(_configOptions);

    // Hidden options, will be allowed both on command line and
    // in config file, but will not be shown to the user.
    //boost::program_options::options_description hidden("Hidden options");
    //hidden.add_options()("input-file", boost::program_options::value< std::vector<std::string> >(), "input-file");
    //_optionItems.add(hidden);

    boost::program_options::store(boost::program_options::parse_command_line(_argc, _argv, _optionItems), _options);
    boost::program_options::notify(_options);

    if (hasOption("help", false))
    {
      displayUsage(std::cout);
      _exit(0);
    }

    if (hasOption("version", false))
    {
      displayVersion(std::cout);
      _exit(0);
    }

    if (hasOption("pid-file", false))
    {
      getOption("pid-file", _pidFile);
      
      try
      {
        boost::filesystem::remove(_pidFile);
        std::ofstream pidFile(_pidFile.c_str());
        pidFile << getpid() << std::endl;
      }
      catch(...)
      {
        TELNYX_LOG_ERROR("Unable to create PID file " << _pidFile);
        _exit(1);
      }
    }

    if (hasOption("daemonize", false))
    {
      if (_pidFile.empty())
      {
        displayUsage(std::cerr);
        std::cerr << std::endl << "ERROR: You must specify pid-file location!" << std::endl;
        std::cerr.flush();
        _exit(-1);
      }
      _isDaemon = true;
    }

    if (hasOption("config-file", false))
    {
      if (getOption("config-file", _configFile) && !_configFile.empty())
      {
        std::ifstream config(_configFile.c_str());
        if (config.good())
        {
          //boost::program_options::store(boost::program_options::parse_config_file(config, _optionItems, true), _options);
          //boost::program_options::notify(_options);
          boost::property_tree::ini_parser::read_ini(_configFile.c_str(), _ptree);
          _hasConfig = true;
        }
        else
        {
          displayUsage(std::cerr);
          std::cerr << std::endl << "ERROR: Unable to open input file " << _configFile << "!" << std::endl;
          std::cerr.flush();
          _exit(-1);
        }
      }
    }

    //
    // Check for required options
    //
    for (std::vector<std::string>::iterator iter = _required.begin(); iter != _required.end(); iter++)
    {
      if (!hasOption(*iter, false))
      {
        std::cout << "Error: Missing required parameter " << *iter << "!" << std::endl;
        displayUsage(std::cout);
        _exit(0);
      }
    }

    if (_procType == Daemon)
      prepareLogger();
  }
  catch(const std::exception& e)
  {
    if (!onParseUnknownOptions(_argc, _argv))
    {
      if(verbose)
        std::cerr << _daemonName << " is not able to parse the options - " << e.what() << std::endl;
      return false;
    }
  }

  return true;
}

inline void ServiceOptions::prepareLogger()
{
  std::string logFile;
  int priorityLevel = 6;
  bool compress = true;
  int purgeCount = _logPurgeCount;
  std::string pattern = _logFormat.empty() ? "%Y-%m-%d %H:%M:%S %s: [%p] %t" : _logFormat;
  std::string times = _logTimes.empty() ? "UTC" : _logTimes;
  
  if (hasOption("log-no-compress", true))
    compress = false;

  getOption("log-purge-count", purgeCount, purgeCount);
  getOption("log-pattern", pattern, pattern);
  getOption("log-times", times, times);

  if (getOption("log-file", logFile) && !logFile.empty())
  {
    if (!getOption("log-level", priorityLevel))
      priorityLevel = 6;
    Telnyx::logger_init(logFile, (Telnyx::LogPriority)priorityLevel, pattern, compress ? "true" : "false", boost::lexical_cast<std::string>(purgeCount), times);
  }
  else
  {
    if (!getOption("log-level", priorityLevel))
      priorityLevel = 6;
    Telnyx::log_reset_level((Telnyx::LogPriority)priorityLevel);
  }
}

inline boost::program_options::options_description* ServiceOptions::options_description(OptionType type)
{
  boost::program_options::options_description* options = 0;
  if (type == GeneralOption)
    options = &_GeneralOptions;
  else if (type == DaemonOption)
    options = &_daemonOptions;
  else if (type == ConfigOption)
    options = &_configOptions;
  else
    assert(false);
  return options;
}

inline void ServiceOptions::addOptionFlag(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  std::ostringstream strm;
  strm << optionName << "," << shortForm;
  options->add_options()(strm.str().c_str(), description.c_str());

}

inline void ServiceOptions::addOptionFlag(const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  options->add_options()(optionName.c_str(), description.c_str());
}

inline void ServiceOptions::addOptionString(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  std::ostringstream strm;
  strm << optionName << "," << shortForm;
  options->add_options()(strm.str().c_str(), boost::program_options::value<std::string>(), description.c_str());
}

inline void ServiceOptions::addOptionString(const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  options->add_options()(optionName.c_str(), boost::program_options::value<std::string>(), description.c_str());
}

inline void ServiceOptions::addOptionStringVector(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  std::ostringstream strm;
  strm << optionName << "," << shortForm;
  options->add_options()(strm.str().c_str(), boost::program_options::value<std::vector<std::string> >(), description.c_str());
}

inline void ServiceOptions::addOptionStringVector(const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  options->add_options()(optionName.c_str(), boost::program_options::value<std::vector<std::string> >(), description.c_str());
}

inline void ServiceOptions::addOptionInt(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  std::ostringstream strm;
  strm << optionName << "," << shortForm;
  options->add_options()(strm.str().c_str(), boost::program_options::value<int>(), description.c_str());
}

inline void ServiceOptions::addOptionInt(const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  options->add_options()(optionName.c_str(), boost::program_options::value<int>(), description.c_str());
}

inline void ServiceOptions::addOptionIntVector(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  std::ostringstream strm;
  strm << optionName << "," << shortForm;
  options->add_options()(strm.str().c_str(), boost::program_options::value<std::vector<int> >(), description.c_str());
}

inline void ServiceOptions::addOptionIntVector(const std::string& optionName, const std::string description, OptionType type)
{
  boost::program_options::options_description* options = options_description(type);
  options->add_options()(optionName.c_str(), boost::program_options::value<std::vector<int> >(), description.c_str());
}

inline void ServiceOptions::displayUsage(std::ostream& strm) const
{
  displayVersion(strm);
  strm << _optionItems;
  strm.flush();
}

inline void ServiceOptions::displayVersion(std::ostream& strm) const
{
  strm << std::endl << _daemonName << " version " << _version << std::endl << "Copyright: " << _copyright << std::endl << std::endl;
  strm.flush();
}

inline std::size_t ServiceOptions::hasOption(const std::string& optionName, bool consolidate) const
{
  if (_isConfigOnly && consolidate)
    return _ptree.count(optionName.c_str());

  std::size_t ct = _options.count(optionName.c_str());
  if (!ct && consolidate && _hasConfig)
    ct = _ptree.count(optionName.c_str());
  return ct;
}

inline size_t ServiceOptions::hasConfigOption(const std::string& optionName) const
{
  return _ptree.count(optionName.c_str());
}

inline bool ServiceOptions::getOption(const std::string& optionName, std::string& value, const std::string& defValue) const
{
  if (defValue.empty() && !hasOption(optionName, false))
  {
    //
    // Check if ptree has it
    //
    if (_hasConfig)
    {
      try
      {
        value = _ptree.get<std::string>(optionName.c_str());
        return true;
      }catch(...)
      {
        return false;
      }
    }
    else
    {
      return false;
    }
  }else if (!hasOption(optionName, false))
  {
    value = defValue;
    return true;
  }

  value = _options[optionName.c_str()].as<std::string>();
  return true;
}

inline bool ServiceOptions::getOption(const std::string& optionName, std::vector<std::string>& value) const
{
  if (!hasOption(optionName, false))
    return false;
  value = _options[optionName.c_str()].as<std::vector<std::string> >();
  return true;
}

inline bool ServiceOptions::getOption(const std::string& optionName, int& value) const
{
  if (!hasOption(optionName, false))
  {
    //
    // Check if ptree has it
    //
    if (_hasConfig)
    {
      try
      {
        value = _ptree.get<int>(optionName.c_str());
        return true;
      }catch(...)
      {
        return false;
      }
    }
    else
    {
      return false;
    }
  }
  else
  {
    value = _options[optionName.c_str()].as<int>();
  }
  return true;
}

inline bool ServiceOptions::getOption(const std::string& optionName, int& value, int defValue) const
{
  if (!hasOption(optionName, false))
  {
    //
    // Check if ptree has it
    //
    if (_hasConfig)
    {
      try
      {
        value = _ptree.get<int>(optionName.c_str());
        return true;
      }catch(...)
      {
        value = defValue;
      }
    }
    else
    {
      value = defValue;
    }
  }else
  {
    value = _options[optionName.c_str()].as<int>();
  }
  return true;
}

inline bool ServiceOptions::getOption(const std::string& optionName, std::vector<int>& value) const
{
  if (!hasOption(optionName,false))
    return false;
  value = _options[optionName.c_str()].as<std::vector<int> >();
  return true;
}


inline bool ServiceOptions::getOption(const std::string& optionName, bool& value, bool defValue) const
{
  if (!hasOption(optionName, false))
  {
    //
    // Check if ptree has it
    //
    if (_hasConfig)
    {
      try
      {
        std::string str = _ptree.get<std::string>(optionName.c_str());
        value = defValue;
        if (!str.empty())
        {
          char ch = str.at(0);
          value = (ch == '1' || ch == 't' || ch == 'T');
        }
        return true;
      }catch(...)
      {
        value = defValue;
      }
    }
    else
    {
      value = defValue;
    }
  }else
  {
    value = _options[optionName.c_str()].as<bool>();
  }
  return true;
}


inline void ServiceOptions::addRequiredString(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionString(shortForm, optionName, description, type);
}

inline void ServiceOptions::addRequiredString(const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionString(optionName, description, type);
}

inline void ServiceOptions::addRequiredInt(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionInt(shortForm, optionName, description, type);
}

inline void ServiceOptions::addRequiredInt(const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionInt(optionName, description, type);
}

inline void ServiceOptions::addRequiredStringVector(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionStringVector(shortForm, optionName, description, type);
}
inline void ServiceOptions::addRequiredStringVector(const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionStringVector(optionName, description, type);
}

inline void ServiceOptions::addRequiredIntVector(char shortForm, const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionIntVector(shortForm, optionName, description, type);
}
inline void ServiceOptions::addRequiredIntVector(const std::string& optionName, const std::string description, OptionType type)
{
  _required.push_back(optionName);
  addOptionIntVector(optionName, description, type);
}

inline ServiceOptions::~ServiceOptions()
{
}

inline pid_t  ServiceOptions::writePidFile(const char* pidFile, bool exclusive)
{
  int handle = open(pidFile, O_RDWR|O_CREAT, 0600);
  if (handle == -1)
  {
    return 0;
  }
  
  if (exclusive && lockf(handle,F_TLOCK,0) == -1)
  {
    return 0;
  }
  
  pid_t pid = getpid();
  
  char pidStr[10];
  sprintf(pidStr,"%d\n", pid);
  if (write(handle, pidStr, strlen(pidStr)) == -1)
  {
    pid = 0;
  }
  
  return pid;
}

inline void  ServiceOptions::daemonize(int argc, char** argv, bool isDaemon)
{
  isDaemon = false;
  for (int i = 0; i < argc; i++)
  {
    std::string arg = argv[i];
    if (arg == "-D" || arg == "--daemonize")
    {
      isDaemon = true;
      break;
    }
  }

  if (isDaemon)
  {
     int pid = 0;
   if(getppid() == 1)
     return;
   pid=fork();
   if (pid<0) exit(1); /* fork error */
   if (pid>0) exit(0); /* parent exits */
   /* child (daemon) continues */
   setsid(); /* obtain a new process group */

   for (int descriptor = getdtablesize();descriptor >= 0;--descriptor)
   {
     close(descriptor); /* close all descriptors we have inheritted from parent*/
   }

   //
   // int h = open("/dev/null",O_RDWR); dup(h); dup(h); /* handle standard I/O */
   //
   
   ::close(STDIN_FILENO);
   ::close(STDOUT_FILENO);
   ::close(STDERR_FILENO);
  }
}

inline void ServiceOptions::catch_global()
{
#define _catch_global_print(msg)  \
  std::ostringstream bt; \
  bt << msg << std::endl; \
  void* trace_elems[20]; \
  int trace_elem_count(backtrace( trace_elems, 20 )); \
  char** stack_syms(backtrace_symbols(trace_elems, trace_elem_count)); \
  for (int i = 0 ; i < trace_elem_count ; ++i ) \
    bt << stack_syms[i] << std::endl; \
  TELNYX_LOG_CRITICAL( bt.str().c_str()); \
  std::cerr << bt.str().c_str(); \
  free(stack_syms);

  try
  {
      throw;
  }
  catch (std::string& e)
  {
    _catch_global_print(e.c_str());
  }
#ifdef MONGO_assert
  catch (mongo::DBException& e)
  {
    _catch_global_print(e.toString().c_str());
  }
#endif
  catch (boost::exception& e)
  {
    _catch_global_print(boost::diagnostic_information(e).c_str());
  }
  catch (std::exception& e)
  {
    _catch_global_print(e.what());
  }
  catch (...)
  {
    _catch_global_print("Error occurred. Unknown exception type.");
  }

  std::abort();
}


inline void ServiceOptions::waitForTerminationRequest()
{
	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);
	sigaddset(&sset, SIGQUIT);
	sigaddset(&sset, SIGTERM);
	sigprocmask(SIG_BLOCK, &sset, NULL);
	int sig;
	sigwait(&sset, &sig);
  std::cout << "Termination Signal RECEIVED" << std::endl;
}

inline int ServiceOptions::argc()
{
  return _argc;
}

inline char** ServiceOptions::argv()
{
  return _argv;
}

inline void ServiceOptions::setLogFormat(const std::string& logFormat)
{
  _logFormat = logFormat;
}

inline const std::string&  ServiceOptions::getLogFormat() const
{
  return _logFormat;
}

inline void ServiceOptions::setLogTimes(const std::string& logTimes)
{
  _logTimes = logTimes;
}

inline const std::string& ServiceOptions::getLogTimes() const
{
  return _logTimes;
}

inline void ServiceOptions::setLogPurgeCount(int purgeCount)
{
  _logPurgeCount = purgeCount;
}

inline int ServiceOptions::getLogPurgeCount()
{
  return _logPurgeCount;
}

} // OSS


#endif	// TELNYX_SERVICEOPTIONS_H_INCLUDED

