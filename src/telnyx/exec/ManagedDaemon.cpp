// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#include "Telnyx/Exec/ManagedDaemon.h"


namespace Telnyx {
namespace Exec {
    
std::string ManagedDaemon::get_queue_path(char type, const std::string& runDir, const std::string& alias)
{
  std::ostringstream path;
  path << runDir << "/" << alias << "-" << type << DAEMON_IPC_QUEUE_SUFFIX;
  return path.str();
}


ManagedDaemon::ManagedDaemon(int argc, char** argv, const std::string& daemonName) :
  ServiceDaemon(argc, argv, daemonName),
  _pIPC(0)
{
}

ManagedDaemon::~ManagedDaemon()
{
  delete _pIPC;
}

int ManagedDaemon::pre_initialize()
{
  int ret = ServiceDaemon::pre_initialize();
  if (ret != 0)
    return ret;
    
  _pIPC = new DaemonIPC(*this, ManagedDaemon::get_queue_path('0', getRunDirectory(), getProcName()),  ManagedDaemon::get_queue_path('1', getRunDirectory(), getProcName()));
  
  return 0;
}

bool ManagedDaemon::sendIPCMessage(const json::Object& message)
{
  if (!_pIPC)
    return false;
  return _pIPC->sendIPCMessage(message);
}
 
  
} } // Telnyx::Exec




