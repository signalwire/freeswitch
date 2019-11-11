// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_EXEC_PROCESS_H_INCLUDED
#define	TELNYX_EXEC_PROCESS_H_INCLUDED

#include "Telnyx/UTL/Thread.h"
#include "Telnyx/Exec/Command.h"
#include <unistd.h>
#include <signal.h>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>


namespace Telnyx {
namespace Exec {

class TELNYX_API Process  : boost::noncopyable
{
public:
  enum Action
  {
    ProcessNormal,
    ProcessRestart,
    ProcessBackoff,
    ProcessShutdown,
    ProcessUnmonitor
  };

  typedef boost::function<Action(int)> DeadProcHandler;
  typedef boost::function<Action(int, double)> MemViolationHandler;
  typedef boost::function<Action(int, double)> CpuViolationHandler;

  Process(const std::string& processName, const std::string& startupCommand, const std::string& shutdownCommand = "", const std::string& pidFile = "");
  ~Process();
  bool execute();
  bool executeAndMonitor();
  bool executeAndMonitorMem(double maxMemPercent);
  bool executeAndMonitorCpu(double maxCpuPercent);
  bool executeAndMonitorMemCpu(double maxMemPercent, double maxCpuPercent);
  pid_t pollPid(const std::string& process, const std::string& pidFile, int maxIteration, long interval);
  int kill(int signal);
  bool shutDown(int signal = SIGTERM);
  bool restart();
  void unmonitor();
  bool exists(double& currentMem, double& currentCpu);

  DeadProcHandler deadProcHandler;
  Action onDeadProcess(int consecutiveHits);
  MemViolationHandler memViolationHandler;
  Action onMemoryViolation(int consecutiveHits, double mem);
  CpuViolationHandler cpuViolationHandler;
  Action onCpuViolation(int consecutiveHits, double cpu);

  static int countProcessInstances(const std::string& process);
  static int getProcessId(const std::string& process);
  static void killAll(const std::string& process, int signal);
  static void killAllDefunct(const std::string& process);
  bool isAlive() const;
  bool& noChild();
  void setInitializeWait(unsigned int ms);
  pid_t getPID() const;
  void setDeadProcAction(Action action);
protected:
  void internalExecuteAndMonitor(int intialWait);
  std::string _processName;
  std::string _startupCommand;
  std::string _shutdownCommand;
  std::string _pidFile;
  pid_t _pid;
  boost::thread* _pMonitorThread;
  Telnyx::semaphore _frequencySync;
  Telnyx::semaphore _backoffSync;
  Telnyx::semaphore _pidSync;
  unsigned int _frequencyTime;
  unsigned int _backoffTime;
  unsigned int _maxIteration;
  unsigned int _deadProcessIteration;
  unsigned int _maxMemViolationIteration;
  unsigned int _maxCpuViolationIteration;
  double _maxCpuUsage;
  double _maxMemUsage;
  bool _unmonitor;
  bool _monitored;
  bool _isAlive;
  bool _noChild;
  unsigned int _initWait;
  Action _deadProcAction;
};

//
// Inlines
//

inline bool Process::isAlive() const
{
  return _isAlive;
}

inline bool& Process::noChild()
{
  return _noChild;
}

inline void Process::setInitializeWait(unsigned int ms)
{
  _initWait = ms;
}

inline pid_t Process::getPID() const
{
  return _pid;
}

inline void Process::setDeadProcAction(Action action)
{
  _deadProcAction = action;
}

} }  // Telnyx::Exec

#endif	// TELNYX_EXEC_PROCESS_H_INCLUDED



