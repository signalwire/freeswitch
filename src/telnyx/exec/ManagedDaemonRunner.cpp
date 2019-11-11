// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/Logger.h"
#include "Telnyx/Exec/ManagedDaemonRunner.h"


namespace Telnyx {
namespace Exec {


const int STOP_YIELD_TIME = 100; /// yield time in milliseconds
const int STOP_MAX_YIELD_TIME = 15000; /// maximum yield time in milliseconds


ManagedDaemonRunner::ManagedDaemonRunner(
  const std::string& executablePath, 
  const std::string& alias,
  const std::string& runDirectory,
  const std::string& startupScript,
  const std::string& shutdownScript,
  const std::string& pidFile,
  int maxRestart) :
    IPCJsonBidirectionalQueue(ManagedDaemon::get_queue_path('1', runDirectory, alias), ManagedDaemon::get_queue_path('0', runDirectory, alias)),
    _path(executablePath),
    _alias(alias),
    _runDirectory(runDirectory),
    _maxRestart(maxRestart),
    _pProcess(0)
{
  boost::filesystem::path path(_path.c_str());
  std::string prog = Telnyx::boost_file_name(path);
  
  if (prog.empty())
  {
    TELNYX_LOG_ERROR("ManagedDaemonRunner::ManagedDaemonRunner - Unable to parse executable from " << _path);
    return;
  }
  
  //Process(const std::string& processName, const std::string& startupCommand, const std::string& shutdownCommand = "", const std::string& pidFile = "");
  _pProcess = new Process(prog, startupScript, shutdownScript, pidFile);
    
}

ManagedDaemonRunner::~ManagedDaemonRunner()
{
  stop();
  delete _pProcess;
}
  
bool ManagedDaemonRunner::readMessage(std::string& message, bool blocking)
{
  return false;
}

bool ManagedDaemonRunner::start()
{
  TELNYX_ASSERT(_pProcess);
  stop(); /// stop all lingering process
  return _pProcess->executeAndMonitor();
}

bool ManagedDaemonRunner::stop()
{
  //
  // Unmonitor the process so it doesn't get restarted
  //
  _pProcess->unmonitor();
  
  //
  // Send the process a SIGTERM
  //
  Process::killAll(_alias, SIGTERM);
  int totalYieldTime = 0;
  for (pid_t pid = getProcessId(); (pid = getProcessId()) == -1 && totalYieldTime < STOP_MAX_YIELD_TIME; totalYieldTime += STOP_YIELD_TIME)
  {
    Telnyx::thread_sleep(STOP_YIELD_TIME); // sleep for 100 milliseconds
  }
  
  //
  // Send the process a SIGKILL
  //
  Process::killAll(_alias, SIGKILL);
  totalYieldTime = 0;
  for (pid_t pid = getProcessId(); (pid = getProcessId()) == -1 && totalYieldTime < STOP_MAX_YIELD_TIME; totalYieldTime += STOP_YIELD_TIME)
  {
    Telnyx::thread_sleep(STOP_YIELD_TIME); // sleep for 100 milliseconds
  }

  return getProcessId() == -1;
}



bool ManagedDaemonRunner::restart()
{
  if (!stop())
    return false;
  return start();
}

Process::Action ManagedDaemonRunner::onDeadProcess(int consecutiveCount)
{
  if (_maxRestart > consecutiveCount)
    return Process::ProcessRestart;
  else
    return Process::ProcessBackoff;
}

void ManagedDaemonRunner::onReceivedIPCMessage(const json::Object& params)
{
  //
  // Does nothing
  //
}
  
} } // Telnyx::Exec