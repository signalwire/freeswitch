/*

The contents of this file are subject to the Mozilla Public License
Version 1.1 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is MP4 Helper Library to Freeswitch MP4 module.

The Initial Developer of the Original Code is 
Paulo Rogério Panhoto <paulo@voicetechnology.com.br>.
Portions created by the Initial Developer are Copyright (C)
the Initial Developer. All Rights Reserved.

*/

#ifndef EXCEPTION_HPP_
#define EXCEPTION_HPP_

#include <exception>
#include <string>

class Exception: public std::exception {
  public:
	Exception()
	{
	}
	
	Exception(const std::string & message): message_(message)
	{
	}
	
	Exception(const std::exception & e): message_(e.what())
	{
	}
	
	Exception(const Exception & e): message_(e.message_)
	{
	}
	
	virtual ~Exception() throw()
	{
	}
	
	const char * what() const throw()
	{
		return message_.c_str();
	}
	
  private:
	std::string message_;
};

#endif