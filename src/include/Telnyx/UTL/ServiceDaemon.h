// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_SERVICEDAEMON_INCLUDED
#define TELNYX_SERVICEDAEMON_INCLUDED

#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/Application.h"
#include "Telnyx/UTL/ServiceOptions.h"
#include "Telnyx/UTL/IPCQueue.h"


namespace Telnyx {

class TELNYX_API ServiceDaemon : public ServiceOptions
{
public:
  ServiceDaemon(int argc, char** argv, const std::string& daemonName);
  virtual ~ServiceDaemon();

  
  virtual int initialize() = 0;
  virtual int main() = 0;
  virtual int pre_initialize();
  virtual int post_initialize();
  virtual int post_main();
  
  const std::string& getProcPath() const;
  const std::string& getProcName() const;
  const std::string& getRunDirectory() const;
  
protected:
  std::string _procPath;
  std::string _procName;
  std::string _runDir;
  friend int main(int argc, char** argv);

};

//
// Inlines
//

inline const std::string& ServiceDaemon::getProcPath() const
{
  return _procPath;
}

inline const std::string& ServiceDaemon::getProcName() const
{
  return _procName;
}

inline const std::string& ServiceDaemon::getRunDirectory() const
{
  return _runDir;
}

} // OSS

#define DAEMONIZE(Daemon, daemonName) \
int main(int argc, char** argv) \
{ \
  bool isDaemon = false; \
  Telnyx::ServiceOptions::daemonize(argc, argv, isDaemon); \
  Telnyx::TELNYX_init(); \
  Daemon daemon(argc, argv, daemonName); \
  int ret = 0; \
  ret = daemon.pre_initialize(); \
  if (ret != 0) \
    exit(ret); \
  ret = daemon.initialize(); \
  if (ret != 0) \
    exit(ret); \
  ret = daemon.main(); \
  if ( ret != 0) \
    exit(ret); \
  ret = daemon.post_main(); \
  if ( ret != 0) \
    exit(ret); \
  exit(0); \
}


#endif // TELNYX_SERVICEDAEMON_INCLUDED


