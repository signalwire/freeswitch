// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_COMMAND_H_INCLUDED
#define	TELNYX_COMMAND_H_INCLUDED

#include <string>
#include <sstream>
#include <vector>


#include "Telnyx/UTL/CoreUtils.h"

namespace Telnyx {
namespace Exec {
    
class TELNYX_API Command : boost::noncopyable
{
public:
  Command();
  Command(const std::string& command);
  Command(const std::string& command, const std::vector<std::string>& args);
  ~Command();
  bool execute(const std::string& command);
  bool execute();
  char readChar() const;
  std::string readLine() const;
  bool isEOF() const;
  bool isGood() const;
  void join() const;
  void join(std::vector<std::string>& output);
  bool kill();
  bool kill(int signal);
  void close();
  void setCommand(const std::string& command);
  const std::string& getCommand() const;
  bool exited();
private:
  TELNYX_HANDLE _pstream;
  std::string _command;
};


//
// Inlines
//

inline void Command::setCommand(const std::string& command)
{
  _command = command;
}

inline const std::string& Command::getCommand() const
{
  return _command;
}


} } // Telnyx::Exec

#define TELNYX_EXEC(cmd) \
{ \
  std::ostringstream strm; \
  strm << cmd; \
  Telnyx::Exec::Command command; \
  if (command.execute(strm.str())) \
    command.join(); \
}

#define TELNYX_EXEC_EX(cmd, output) \
{ \
  std::ostringstream strm; \
  strm << cmd; \
  Telnyx::Exec::Command command; \
  if (command.execute(strm.str())) \
    command.join(output); \
}


#endif	/* TELNYX_EXEC_H_INCLUDED */

