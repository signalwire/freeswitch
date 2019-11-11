// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/ServiceDaemon.h"
#include "Telnyx/Exec/Command.h"
#include "Telnyx/Exec/Process.h"
#include <fstream>


namespace Telnyx {



ServiceDaemon::ServiceDaemon(int argc, char** argv, const std::string& daemonName) :
  ServiceOptions(argc, argv, daemonName)
{
  _procPath = argv[0];
  boost::filesystem::path procPath(_procPath.c_str());
  _procName = Telnyx::boost_file_name(procPath);
  
  addDaemonOptions();
  addOptionString("run-directory", "The directory where application data would be stored.");
}

ServiceDaemon::~ServiceDaemon()
{
}


int ServiceDaemon::pre_initialize()
{
  //
  // Parse command line options
  //
  if (!parseOptions())
    return -1;
  
  
  std::string newRunDir;
  if (getOption("run-directory", newRunDir) && !newRunDir.empty())
  {
    _runDir = newRunDir;
  }
  else
  {
    _runDir = Telnyx::boost_path(boost::filesystem::current_path());
  }
  
  TELNYX_ASSERT(!_runDir.empty());
  
  ::chdir(_runDir.c_str());
  
  return 0;
}



int ServiceDaemon::post_initialize()
{
  return 0;
}

int ServiceDaemon::post_main()
{
  return 0;
}


}
