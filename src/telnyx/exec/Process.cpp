// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/Exec/Process.h"
#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/Logger.h"
#include <boost/algorithm/string.hpp>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>


#define PROC_INIT_WAIT 10000


namespace Telnyx {
namespace Exec {


static bool headerSplit(
    const std::string & header,
    std::string & name,
    std::string & value)
{
  size_t nameBound = header.find_first_of(':');
  if (nameBound == std::string::npos)
  {
    return false;
  }
  name = header.substr(0,nameBound);
  value = header.substr(nameBound+1);
  boost::trim(name);
  boost::trim(value);
  return true;
}

static bool getStatusHeader(const std::string& file, const std::string& name_, std::string& value_)
{
  std::ifstream procif(file.c_str());
  if (!procif.is_open())
    return false;

  std::string headerName = name_;
  boost::to_lower(headerName);

  
  while(!procif.eof() && !procif.bad())
  {
    std::string line;
    std::getline(procif, line);
    std::string name;
    std::string value;
    if (headerSplit(line, name, value))
    {
      boost::to_lower(name);
      if (name == headerName)
      {
        value_ = value;
        return true;
      }
    }
  }

  return false;
}

static bool getStatusHeader(const std::string& file, const std::string& name_, std::vector<std::string>& values_)
{
  std::ifstream procif(file.c_str());
  if (!procif.is_open())
    return false;

  std::string headerName = name_;
  boost::to_lower(headerName);

  values_.clear();
  while(!procif.eof() && !procif.bad())
  {
    std::string line;
    std::getline(procif, line);
    std::string name;
    std::string value;
    if (headerSplit(line, name, value))
    {
      boost::to_lower(name);
      if (name == headerName)
      {
        values_.push_back(value);
      }
    }
    line.clear();
  }

  return !values_.empty();
}

static unsigned int kbToInt(const std::string& kb)
{
  std::vector<std::string> tokens = Telnyx::string_tokenize(kb, " ");
  if (tokens.size() < 2)
    return 0;
  return Telnyx::string_to_number<unsigned int>(tokens[0].c_str());
}

double getMem(int pid)
{
  std::string memTotal;
  if (!getStatusHeader("/proc/meminfo", "memtotal", memTotal))
    return 0.0;

  std::ostringstream smapFile;
  smapFile << "/proc/" << pid << "/smaps";

  std::vector<std::string> memBlocks;
  if (!getStatusHeader(smapFile.str().c_str(), "Private_Dirty", memBlocks))
    return 0.0;

  unsigned int procMem = 0;
  unsigned int totalMem = Telnyx::string_to_number<unsigned int>(memTotal.c_str());
  for (std::vector<std::string>::iterator iter = memBlocks.begin(); iter != memBlocks.end(); iter++)
    procMem += kbToInt(*iter);


  if (totalMem == 0 || procMem == 0)
    return 0.0;

  return (double)procMem / (double)totalMem;
}

Process::Process(const std::string& processName, const std::string& startupCommand,
  const std::string& shutdownCommand, const std::string& pidFile) :
  _processName(processName),
  _startupCommand(startupCommand),
  _shutdownCommand(shutdownCommand),
  _pidFile(pidFile),
  _pid(-1),
  _pMonitorThread(0),
  _frequencyTime(1000),
  _backoffTime(_frequencyTime*10),
  _maxIteration(5),
  _deadProcessIteration(0),
  _maxMemViolationIteration(0),
  _maxCpuViolationIteration(0),
  _maxCpuUsage(0.0),
  _maxMemUsage(0.0),
  _unmonitor(false),
  _monitored(false),
  _isAlive(false),
  _noChild(false),
  _initWait(0),
  _deadProcAction(ProcessRestart)
{
  
}

Process::~Process()
{
  unmonitor();
}

pid_t Process::pollPid(const std::string& process, const std::string& pidFile, int maxIteration, long interval)
{
  pid_t pid = -1;
  int iter = 0;

  do
  {
    boost::filesystem::path fp(pidFile);
    if (boost::filesystem::exists(fp))
    {
      std::string buff;
      std::ifstream ifstrm(pidFile.c_str());
      if (std::getline(ifstrm, buff))
      {
        pid = (pid_t)Telnyx::string_to_number<int>(buff.c_str());
        std::ostringstream procd;
        procd << "/proc/" << pid;
        if (boost::filesystem::exists(procd.str().c_str()))
        {
          procd << "/status";
          std::string procName;

          if (getStatusHeader(procd.str().c_str(), "name", procName) && Telnyx::string_starts_with(process, procName.c_str()))
          {
            return pid;
          }
        }
      }
    }
  } while (!_pidSync.wait(interval) && ++iter < maxIteration);
  return -1;
}

void Process::unmonitor()
{
  _unmonitor = true;
  _frequencySync.set();
  _backoffSync.set();
  _pidSync.set();
  if (_pMonitorThread)
  {
    if (_pMonitorThread->joinable())
      _pMonitorThread->join();
    delete _pMonitorThread;
    _pMonitorThread = 0;
  }
}

bool Process::execute()
{

  if (_pid != -1)
    return false;

  if (_startupCommand.empty())
    return false;

  //
  // First check if the process is alaready running
  //
  if (!_pidFile.empty() && !_processName.empty())
  {
     _pid = pollPid(_processName, _pidFile, 1, 1000);
  }

  if (_pid != -1)
  {
    TELNYX_LOG_DEBUG("Process monitor attaching to existing process " << _processName << " with PID=" << _pid);
    return true;
  }

  pid_t pid = fork();
  if (pid == (pid_t)0)
  {
    std::vector<std::string> tokens;
    boost::split(tokens, _startupCommand, boost::is_any_of(" "), boost::token_compress_on);
    if (tokens.size() >= 2)
    {
      TELNYX_LOG_DEBUG("Spawning " << _startupCommand);
      char** argv = (char**)calloc((tokens.size()+1), sizeof(char **));
      argv[0] = (char *) malloc(tokens[0].size()+1);
      strcpy( argv[0], tokens[0].c_str());

      size_t i = 1;
      for (i = 1; i < tokens.size(); i++)
      {
        std::string& token = tokens[i];
        argv[i] = (char *) malloc(token.size()+1);
        strcpy( argv[i], token.c_str());
      }
      argv[i] = (char*) 0;
      if (execv(tokens[0].c_str(), argv) == -1)
      {
        TELNYX_LOG_ERROR("Failed to execute " << _startupCommand << " ERROR: " << strerror(errno));
        free(argv);
        return false;
      }
      free(argv);
    }
    else if (tokens.size() == 1)
    {
      TELNYX_LOG_DEBUG("Spawning " << _startupCommand);
      if (execl(tokens[0].c_str(), "", NULL) == -1)
      {
        TELNYX_LOG_ERROR("Failed to execute " << _startupCommand << " ERROR: " << strerror(errno));
        return false;
      }
    }
    else
    {
      assert(false);
    }
  }
  else if (pid != (pid_t) -1)
  {
    if (!_noChild)
    {
      pid_t foundpid;
      do {
      foundpid = wait3(NULL, 0, NULL);
      if (foundpid == (pid_t) -1 && errno == EINTR)
      {
          continue;
      }
      } while (foundpid != (pid_t) -1 && foundpid != pid);
    }
    //
    // this is the parent;
    //
    if (!_pidFile.empty() && !_processName.empty())
    {
       _pid = pollPid(_processName, _pidFile, 10, 1000);
    }
    else
    {
      _pid = pid;
    }

    if (_pid != -1)
    {
      TELNYX_LOG_DEBUG( "Finished spawning process " << _processName << " PID=" << _pid);
    }
    else
    {
      TELNYX_LOG_ERROR("Unable to spawn " << _processName);
    }
  }
  else
  {
    TELNYX_LOG_ERROR("Unable to spawn " << _processName);
  }
  return true;
}


Process::Action Process::onDeadProcess(int consecutiveHits)
{
  if (deadProcHandler)
    return deadProcHandler(consecutiveHits);
  return _deadProcAction;
}

Process::Action Process::onMemoryViolation(int consecutiveHits, double mem)
{
  if (memViolationHandler)
    return memViolationHandler(consecutiveHits, mem);
  return ProcessRestart;
}

Process::Action Process::onCpuViolation(int consecutiveHits, double cpu)
{
  if (cpuViolationHandler)
    return cpuViolationHandler(consecutiveHits, cpu);
  return ProcessRestart;
}


bool Process::exists(double& currentMem, double& currentCpu)
{
  std::ostringstream procd;
  procd << "/proc/" << _pid;
  bool ok = false;
  if (boost::filesystem::exists(procd.str().c_str()))
  {
    procd << "/status";
    std::string procName;
    if (!_processName.empty())
      ok = (getStatusHeader(procd.str().c_str(), "name", procName) && Telnyx::string_starts_with(_processName, procName.c_str()));
    else
      ok = true;
  }

#if 0
  if (ok)
    currentMem = getMem(_pid);
#endif
  
  return ok;
}

void Process::internalExecuteAndMonitor(int initialWait)
{
  TELNYX_LOG_DEBUG("Started monitoring " << _processName);
  _monitored = true;
  _deadProcessIteration = 0;
  _maxMemViolationIteration = 0;
  _maxCpuViolationIteration = 0;
  double currentMem = 0.0;
  double currentCpu = 0.0;

  if (initialWait > 0)
  {
    if (_frequencySync.wait(initialWait))
    {
      //
      // It was signalled before we even started
      //
      return;
    }
  }

  while (!_unmonitor && !_frequencySync.wait(_frequencyTime))
  {
    bool alive = exists(currentMem, currentCpu);

    Action action = ProcessNormal;
    if (!alive)
    {
      _isAlive = false;
      ++_deadProcessIteration;
      TELNYX_LOG_WARNING("Process monitor detected dead process "  << _processName << " PID=" << _pid);
      action = onDeadProcess(_deadProcessIteration);

      if (!_pidFile.empty())
        boost::filesystem::remove(_pidFile);
    }
    else /// Its alive
    {
      _isAlive = true;
      _deadProcessIteration = 0;
      //
      // We found the process, now check its memory consumption
      //
      if (_maxMemUsage > 0.0 && currentMem > _maxMemUsage)
      {
        action = onMemoryViolation(++_maxMemViolationIteration, currentMem);
      }
      else
      {
        _maxMemViolationIteration = 0;
      }
      //
      // We found the process, now check its memory consumption
      //
      if (_maxCpuUsage > 0.0 && currentCpu > _maxCpuUsage)
      {
        action = onCpuViolation(++_maxCpuViolationIteration, currentCpu);
      }
      else
      {
        _maxCpuViolationIteration = 0;
      }
    }

    if (action == ProcessNormal)
    {
      //
      // Everything is fine
      //
      continue;
    }
    else if (action == ProcessRestart)
    {
      TELNYX_LOG_DEBUG("Process monitor is restarting process "  << _processName << " PID=" << _pid);
      if (!alive)
      {
        _pid = -1;
        if (!execute())
          continue;
      }
      else
      {
        if (!restart())
          continue;
      }
      TELNYX_LOG_DEBUG("Process monitor monitoring process " << _processName << " PID=" << _pid);
    }
    else if (action == ProcessBackoff)
    {
      if (!_backoffSync.wait(_backoffTime))
      {
        if (!alive)
        {
          _pid = -1;
          if (!execute())
            continue;
        }
        else
        {
          if (!restart())
            continue;
        }
        TELNYX_LOG_DEBUG("Process monitor monitoring process " << _processName << " PID=" << _pid);
      }
    }
    else
    {
      //
      // This will exit the thread but not delete it
      //
      _unmonitor = true;
      _pid = -1;
      break;
    }
  }

  TELNYX_LOG_DEBUG("Stopped monitoring " << _processName);
}

int Process::countProcessInstances(const std::string& process)
{
  int instances = 0;
  if ( !boost::filesystem::exists("/proc"))
    return 0;

  boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
  for (boost::filesystem::directory_iterator iter("/proc"); iter != end_itr; ++iter)
  {
    if (boost::filesystem::is_directory(iter->status()))
    {
      if (Telnyx::boost_file_name(iter->path()) != "self" && Telnyx::boost_file_name(iter->path()) != "thread-self")
      {
        boost::filesystem::path statusFile = operator/(iter->path(), "status");
        std::string procName;
        if (getStatusHeader(Telnyx::boost_path(statusFile), "name", procName) && Telnyx::string_starts_with(process, procName.c_str()))
        {
          ++instances;
        }
      }
    }
  }
  return instances;
}


pid_t Process::getProcessId(const std::string& process)
{
  if ( !boost::filesystem::exists("/proc"))
    return -1;

  boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
  for (boost::filesystem::directory_iterator iter("/proc"); iter != end_itr; ++iter)
  {
    if (boost::filesystem::is_directory(iter->status()))
    {

      boost::filesystem::path statusFile = operator/(iter->path(), "status");
      std::string procName;
      if (getStatusHeader(Telnyx::boost_path(statusFile), "name", procName) && Telnyx::string_starts_with(process, procName.c_str()))
        return Telnyx::string_to_number<pid_t>(Telnyx::boost_file_name(iter->path()).c_str());
    }
  }
  return -1;
}

void Process::killAllDefunct(const std::string& process)
{
  //
  // Implement this
  //
}

void Process::killAll(const std::string& process, int signal)
{
  if ( !boost::filesystem::exists("/proc"))
    return;

  boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
  for (boost::filesystem::directory_iterator iter("/proc"); iter != end_itr; ++iter)
  {
    if (boost::filesystem::is_directory(iter->status()))
    {
      if (Telnyx::boost_file_name(iter->path()) != "self")
      {
        boost::filesystem::path statusFile = operator/(iter->path(), "status");
        std::string procName;
        if (getStatusHeader(Telnyx::boost_path(statusFile), "name", procName) && Telnyx::string_starts_with(process, procName.c_str()))
          ::kill(Telnyx::string_to_number<pid_t>(Telnyx::boost_file_name(iter->path()).c_str()), signal);
      }
    }
  }
}



bool Process::executeAndMonitor()
{
  shutDown();

  if (!execute())
    return false;



  _unmonitor = false;
  _maxCpuUsage = 0.0;
  _maxMemUsage = 0.0;

  _pMonitorThread = new boost::thread(boost::bind(&Process::internalExecuteAndMonitor, this, _initWait ? _initWait : PROC_INIT_WAIT));

  return true;
}

bool Process::executeAndMonitorMem(double maxMemPercent)
{
  shutDown();

  if (!execute())
    return false;

  _unmonitor = false;
  _maxCpuUsage = 0.0;
  _maxMemUsage = maxMemPercent;

  _pMonitorThread = new boost::thread(boost::bind(&Process::internalExecuteAndMonitor, this, _initWait ? _initWait : PROC_INIT_WAIT));

  return true;
}

bool Process::executeAndMonitorCpu(double maxCpuPercent)
{
  shutDown();

  if (!execute())
    return false;

  _unmonitor = false;
  _maxCpuUsage = maxCpuPercent;
  _maxMemUsage = 0.0;

  _pMonitorThread = new boost::thread(boost::bind(&Process::internalExecuteAndMonitor, this, _initWait ? _initWait : PROC_INIT_WAIT));

  return true;
}

bool Process::executeAndMonitorMemCpu(double maxMemPercent, double maxCpuPercent)
{
  shutDown();

  if (!execute())
    return false;

  _unmonitor = false;
  _maxCpuUsage = maxCpuPercent;
  _maxMemUsage = maxCpuPercent;

  _pMonitorThread = new boost::thread(boost::bind(&Process::internalExecuteAndMonitor, this, _initWait ? _initWait : PROC_INIT_WAIT));

  return true;
}

bool Process::shutDown(int signal)
{
  _unmonitor = true;
  if (_pMonitorThread && _pMonitorThread->joinable())
  {
    TELNYX_LOG_DEBUG("Process::shutDown " << _processName << " waiting for monitor thread to exit");
    _pMonitorThread->join();
    delete _pMonitorThread;
    _pMonitorThread = 0;
  }


  if (!_pidFile.empty() && !_processName.empty())
  {
     _pid = pollPid(_processName, _pidFile, 1, 1000);
  }

  if (_pid == -1)
  {
    return true;
  }

  if (_shutdownCommand.empty())
  {
    TELNYX_LOG_DEBUG("Process::shutDown " << _processName << " killing process " << _pid);

    if (::kill(_pid, signal) == 0)
    {
      _pid = -1;
      return true;
    }
    else
    {
      std::cerr << "Failed to execute kill(SIGKILL) for process ID " << _pid << std::endl;
      return false;
    }
  }

  pid_t oldPid = _pid;

  TELNYX_LOG_DEBUG("Process::shutDown " << _processName << " executing shutdown script " << _shutdownCommand);
  Telnyx::Exec::Command command(_shutdownCommand);
  command.execute();
  while (command.isGood() && !command.isEOF())
  {
    std::string line = command.readLine();
    if (line.empty())
      continue;
  }


  if (!_pidFile.empty() && !_processName.empty())
  {
    bool dead = false;
    for (int i = 0; i < 10; i++)
    {
      if  (pollPid(_processName, _pidFile, 1, 1000) != oldPid)
      {
        dead = true;
        break;
      }
    }
    
    if (!dead)
      ::kill(oldPid, SIGKILL);
  }

  _pid = -1;

  TELNYX_LOG_DEBUG("Process::shutDown " << _processName << " ended.");
  return true;
}

bool Process::restart()
{
  if (!shutDown())
    return false;
  if (_monitored)
  {
    _unmonitor = true;
    if (_pMonitorThread && _pMonitorThread->joinable())
    {
      _pMonitorThread->join();
      delete _pMonitorThread;
      _pMonitorThread = 0;
    }

    _unmonitor = false;
    internalExecuteAndMonitor(0);
    return true;
  }
  else
  {
    return execute();
  }
}

} }  // Telnyx::Exec





