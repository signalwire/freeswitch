// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include <vector>
#include <map>
#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/Logger.h"
#include "Poco/Stopwatch.h"
#include "Poco/Timestamp.h"
#include "Poco/LocalDateTime.h"
#include "Poco/DateTimeFormatter.h"
#include "Telnyx/UTL/Thread.h"
#include "Telnyx/UTL/CoreUtils.h"


namespace Telnyx {

static Telnyx::mutex_critic_sec gInitMutex;  
static bool gCalledInit = false;
static bool gCalledDeinit = false;
static char** gArgv = 0;
static int gArgc = 0;

namespace Private {

  
  
  TimedFuncTimer::TimedFuncTimer(const char* fileName, int lineNumber, const char* funcName)
  {
    _fileName = fileName;
    _lineNumber = lineNumber;
    _funcName = funcName;
    _stopWatch = new Poco::Stopwatch();
    _pitStop = 0;
    static_cast<Poco::Stopwatch*>(_stopWatch)->start();
  }

  TimedFuncTimer::~TimedFuncTimer()
  {
    Poco::Stopwatch* stopWatch = static_cast<Poco::Stopwatch*>(_stopWatch);
    Poco::Timestamp::TimeDiff diff = stopWatch->elapsed();
    TELNYX_LOG_NOTICE("Timed Function: " << _fileName << ":" << _lineNumber
            << ":" << _funcName << "() Total Elapsed = " << diff << " microseconds");
    delete stopWatch;
  }

  void TimedFuncTimer::flushElapsed(const char* label)
  {
    Poco::Stopwatch* stopWatch = static_cast<Poco::Stopwatch*>(_stopWatch);
    Poco::Timestamp::TimeDiff diff = stopWatch->elapsed() - _pitStop;
    _pitStop += diff;
    TELNYX_LOG_NOTICE("Timed Function Flush " << label << ": " << _fileName << ":" << _lineNumber
            << ":" << _funcName << "() Elapsed = " << diff << " microseconds");
  }
  
  
}

static std::vector<boost::function<void()> > _initFuncs;
static std::vector<boost::function<void()> > _deinitFuncs;

void TELNYX_API TELNYX_register_init(boost::function<void()> func)
{
  _initFuncs.push_back(func);
}

void TELNYX_API TELNYX_register_deinit(boost::function<void()> func)
{
  _deinitFuncs.push_back(func);
}



void TELNYX_init(int argc, char** argv, bool enableCrashHandling)
{
  Telnyx::mutex_critic_sec_lock lock(gInitMutex);
  if (gCalledInit)
  {
    return;
  }

  gCalledInit = true;
  gArgc = argc;
  gArgv = argv;
  Telnyx::__init_system_dir();

  for (static std::vector<boost::function<void()> >::iterator iter = _initFuncs.begin();
    iter != _initFuncs.end(); iter++) (*iter)();
}

void TELNYX_API TELNYX_init(bool enableCrashHandling)
{
  TELNYX_init(0, 0, enableCrashHandling);
}

void TELNYX_argv(int* argc, char*** argv)
{
  *argc = gArgc;
  *argv = gArgv;
}

void TELNYX_deinit()
{
  Telnyx::mutex_critic_sec_lock lock(gInitMutex);
  if (gCalledDeinit)
  {
    return;
  }
  
  gCalledDeinit = true;
  for (static std::vector<boost::function<void()> >::iterator iter = _deinitFuncs.begin();
    iter != _deinitFuncs.end(); iter++) (*iter)();
  
}



} // OSS

