// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/Exec/Command.h"
#include "Telnyx/Exec/pstream.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <vector>

using namespace redi;

// explicit instantiations of template classes
template class redi::basic_pstreambuf<char>;
template class redi::pstream_common<char>;
template class redi::basic_ipstream<char>;
template class redi::basic_opstream<char>;
template class redi::basic_pstream<char>;
template class redi::basic_rpstream<char>;

#if defined(__sun)
  int sh_cmd_not_found = 1;
#else
  int sh_cmd_not_found = 127;
#endif

namespace Telnyx {
namespace Exec {

Command::Command() :
  _pstream(0)
{
}

Command::Command(const std::string& command) :
  _pstream(0),
  _command(command)
{
}

Command::Command(const std::string& command, const std::vector<std::string>& args) :
  _pstream(0)
{
  std::ostringstream buff;
  buff << command;
  for (std::vector<std::string>::const_iterator iter = args.begin();
    iter != args.end(); iter++)
  {
    buff << " " << *iter;
  }
  _command = buff.str();
}

Command::~Command()
{
  close();
}

void Command::close()
{
  if (_pstream)
  {
    ipstream* pStrm = static_cast<ipstream*>(_pstream);
    pStrm->clear();
    pStrm->close();
    delete pStrm;
    _pstream = 0;
  }
}

bool Command::execute()
{
  close();
  _pstream = new ipstream();
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  pStrm->open(_command);
  return !isEOF() && isGood();
}

bool Command::execute(const std::string& command)
{
  _command = command;
  return execute();
}

char Command::readChar() const
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return 0;
  char c;
  pStrm->get(c);
  return c;
}

std::string Command::readLine() const
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return 0;
  std::string buf;
  std::getline(pStrm->out(), buf);
  return buf;
}

bool Command::isEOF() const
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return true;
  return pStrm->eof();
}

void Command::join() const
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return;
  std::string buf;
  while(!pStrm->eof() && pStrm->good() && std::getline(pStrm->out(), buf));
}

void Command::join(std::vector<std::string>& output)
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return;
  std::string buf;
  while(!pStrm->eof() && pStrm->good() && std::getline(pStrm->out(), buf))
  {
    output.push_back(buf);
    buf.clear();
  }
}

bool Command::isGood() const
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return false;

  return pStrm->good();
}

bool Command::kill()
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return false;

  pstreambuf* pbuf = pStrm->rdbuf();
  if (!pbuf)
    return false;
  pbuf->kill();
  return true;
}

bool Command::kill(int signal)
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return false;

  pstreambuf* pbuf = pStrm->rdbuf();
  if (!pbuf)
    return false;
  pbuf->kill(signal);
  return true;
}


bool Command::exited()
{
  ipstream* pStrm = static_cast<ipstream*>(_pstream);
  if (!pStrm)
    return -1;
  pstreambuf* pbuf = pStrm->rdbuf();
  if (!pbuf)
    return true;
  return pbuf->exited();
}


} } // Telnyx::Exec


