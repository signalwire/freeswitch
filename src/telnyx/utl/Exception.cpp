// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#include <typeinfo>

#include "Telnyx/UTL/Exception.h"


namespace Telnyx {

Exception::Exception(int code): _pNested(0), _code(code)
{
}


Exception::Exception(const std::string& msg, int code): _msg(msg), _pNested(0), _code(code)
{
}


Exception::Exception(const std::string& msg, const std::string& arg, int code): _msg(msg), _pNested(0), _code(code)
{
	if (!arg.empty())
	{
		_msg.append(": ");
		_msg.append(arg);
	}
}


Exception::Exception(const std::string& msg, const Exception& nested, int code): _msg(msg), _pNested(nested.clone()), _code(code)
{
}


Exception::Exception(const Exception& exc):
	std::exception(exc),
	_msg(exc._msg),
	_code(exc._code)
{
	_pNested = exc._pNested ? exc._pNested->clone() : 0;
}

Exception::Exception(const std::exception& exc):
  _pNested(0),
  _code(0)
{
	_msg = exc.what();
}

Exception::Exception(const boost::system::system_error& exc) :
  _pNested(0),
  _code(0)
{
  _msg = exc.what();
  _code = exc.code().value();
}
	
Exception::~Exception() throw()
{
	delete _pNested;
}


Exception& Exception::operator = (const Exception& exc)
{
	if (&exc != this)
	{
		delete _pNested;
		_msg     = exc._msg;
		_pNested = exc._pNested ? exc._pNested->clone() : 0;
		_code    = exc._code;
	}
	return *this;
}


Exception& Exception::operator = (const std::exception& exc)
{
  if (&exc != this)
	{
		delete _pNested;
    _pNested = 0;
    _code = 0;
    _msg = exc.what();
	}
	return *this;
}

Exception& Exception::operator = (const boost::system::system_error& exc)
{
  delete _pNested;
  _pNested = 0;
  _code = exc.code().value();
  _msg = exc.what();
	return *this;
}

const char* Exception::name() const throw()
{
	return "Exception";
}


const char* Exception::className() const throw()
{
	return typeid(*this).name();
}

	
const char* Exception::what() const throw()
{
  if(_msg.empty())
    return name();
  return _msg.c_str();
}

	
std::string Exception::displayText() const
{
	std::string txt = name();
	if (!_msg.empty())
	{
		txt.append(": ");
		txt.append(_msg);
	}
	return txt;
}


Exception* Exception::clone() const
{
	return new Exception(*this);
}


void Exception::rethrow() const
{
	throw *this;
}


TELNYX_IMPLEMENT_EXCEPTION(LogicException, Exception, "Logic exception")
TELNYX_IMPLEMENT_EXCEPTION(AssertionViolationException, LogicException, "Assertion violation")
TELNYX_IMPLEMENT_EXCEPTION(NullPointerException, LogicException, "Null pointer")
TELNYX_IMPLEMENT_EXCEPTION(BugcheckException, LogicException, "Bugcheck")
TELNYX_IMPLEMENT_EXCEPTION(InvalidArgumentException, LogicException, "Invalid argument")
TELNYX_IMPLEMENT_EXCEPTION(NotImplementedException, LogicException, "Not implemented")
TELNYX_IMPLEMENT_EXCEPTION(RangeException, LogicException, "Out of range")
TELNYX_IMPLEMENT_EXCEPTION(IllegalStateException, LogicException, "Illegal state")
TELNYX_IMPLEMENT_EXCEPTION(InvalidAccessException, LogicException, "Invalid access")
TELNYX_IMPLEMENT_EXCEPTION(SignalException, LogicException, "Signal received")
TELNYX_IMPLEMENT_EXCEPTION(UnhandledException, LogicException, "Unhandled exception")

TELNYX_IMPLEMENT_EXCEPTION(RuntimeException, Exception, "Runtime exception")
TELNYX_IMPLEMENT_EXCEPTION(NotFoundException, RuntimeException, "Not found")
TELNYX_IMPLEMENT_EXCEPTION(ExistsException, RuntimeException, "Exists")
TELNYX_IMPLEMENT_EXCEPTION(TimeoutException, RuntimeException, "Timeout")
TELNYX_IMPLEMENT_EXCEPTION(SystemException, RuntimeException, "System exception")
TELNYX_IMPLEMENT_EXCEPTION(RegularExpressionException, RuntimeException, "Error in regular expression")
TELNYX_IMPLEMENT_EXCEPTION(LibraryLoadException, RuntimeException, "Cannot load library")
TELNYX_IMPLEMENT_EXCEPTION(LibraryAlreadyLoadedException, RuntimeException, "Library already loaded")
TELNYX_IMPLEMENT_EXCEPTION(NoThreadAvailableException, RuntimeException, "No thread available")
TELNYX_IMPLEMENT_EXCEPTION(PropertyNotSupportedException, RuntimeException, "Property not supported")
TELNYX_IMPLEMENT_EXCEPTION(PoolOverflowException, RuntimeException, "Pool overflow")
TELNYX_IMPLEMENT_EXCEPTION(NoPermissionException, RuntimeException, "No permission")
TELNYX_IMPLEMENT_EXCEPTION(OutOfMemoryException, RuntimeException, "Out of memory")
TELNYX_IMPLEMENT_EXCEPTION(DataException, RuntimeException, "Data error")

TELNYX_IMPLEMENT_EXCEPTION(DataFormatException, DataException, "Bad data format")
TELNYX_IMPLEMENT_EXCEPTION(SyntaxException, DataException, "Syntax error")
TELNYX_IMPLEMENT_EXCEPTION(CircularReferenceException, DataException, "Circular reference")
TELNYX_IMPLEMENT_EXCEPTION(PathSyntaxException, SyntaxException, "Bad path syntax")
TELNYX_IMPLEMENT_EXCEPTION(IOException, RuntimeException, "I/O error")
TELNYX_IMPLEMENT_EXCEPTION(FileException, IOException, "File access error")
TELNYX_IMPLEMENT_EXCEPTION(FileExistsException, FileException, "File exists")
TELNYX_IMPLEMENT_EXCEPTION(FileNotFoundException, FileException, "File not found")
TELNYX_IMPLEMENT_EXCEPTION(PathNotFoundException, FileException, "Path not found")
TELNYX_IMPLEMENT_EXCEPTION(FileReadOnlyException, FileException, "File is read-only")
TELNYX_IMPLEMENT_EXCEPTION(FileAccessDeniedException, FileException, "Access to file denied")
TELNYX_IMPLEMENT_EXCEPTION(CreateFileException, FileException, "Cannot create file")
TELNYX_IMPLEMENT_EXCEPTION(OpenFileException, FileException, "Cannot open file")
TELNYX_IMPLEMENT_EXCEPTION(WriteFileException, FileException, "Cannot write file")
TELNYX_IMPLEMENT_EXCEPTION(ReadFileException, FileException, "Cannot read file")
TELNYX_IMPLEMENT_EXCEPTION(UnknownURISchemeException, RuntimeException, "Unknown URI scheme")

TELNYX_IMPLEMENT_EXCEPTION(NetException, IOException, "NET Exception");

TELNYX_IMPLEMENT_EXCEPTION(ApplicationException, Exception, "Application exception")
TELNYX_IMPLEMENT_EXCEPTION(BadCastException, RuntimeException, "Bad cast exception")

} // OSS

