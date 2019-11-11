// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef MANAGEDDAEMON_H_INCLUDED
#define	MANAGEDDAEMON_H_INCLUDED


#include "Telnyx/UTL/ServiceDaemon.h"
#include "Telnyx/UTL/IPCQueue.h"


namespace Telnyx {
namespace Exec {
    
  
#define DAEMON_IPC_QUEUE_SUFFIX "-process-control.ipc"  
  
class TELNYX_API ManagedDaemon : public ServiceDaemon
{
public:
  class DaemonIPC : public IPCJsonBidirectionalQueue
  {
  public:
    DaemonIPC(ManagedDaemon& daemon, const std::string& readQueuePath,  const std::string& writeQueuePath) :
      IPCJsonBidirectionalQueue(readQueuePath,writeQueuePath),
      _daemon(daemon)
    {
    }

    void onReceivedIPCMessage(const json::Object& params)
    {
      _daemon.onReceivedIPCMessage(params);
    }

    ManagedDaemon& _daemon;
  };

  ManagedDaemon(int argc, char** argv, const std::string& daemonName);
  virtual ~ManagedDaemon();

  
  int pre_initialize();
  virtual void onReceivedIPCMessage(const json::Object& message) = 0;
  bool sendIPCMessage(const json::Object& message);
  
  static std::string get_queue_path(char type, const std::string& runDir, const std::string& alias);
protected:
  DaemonIPC* _pIPC;
  friend int main(int argc, char** argv);

};

  
  
} } // Telnyx::Exec


#endif	// MANAGEDDAEMON_H_INCLUDED

