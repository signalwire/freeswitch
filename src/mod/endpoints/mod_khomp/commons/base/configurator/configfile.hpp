/*
    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2009 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License Version 1.1
  (the "License"); you may not use this file except in compliance with the
  License. You may obtain a copy of the License at http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file under
  the MPL, indicate your decision by deleting the provisions above and replace them
  with the notice and other provisions required by the LGPL License. If you do not
  delete the provisions above, a recipient may use your version of this file under
  either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <vector>
#include <fstream>
#include <sstream>
#include <set>

#include <format.hpp>

#include <configurator/section.hpp>

#ifndef _CONFIG_CONFIGFILE_HPP_
#define _CONFIG_CONFIGFILE_HPP_

struct Configfile: public Section
{
    typedef std::list < std::string >  ErrorVector;
    typedef std::set  < std::string >  NameSet;

    Configfile(const std::string & name, const std::string & desc)
    : Section(name, desc), _good(false) {};

    virtual ~Configfile() {};

    bool                      good() const { return _good;     };
    const std::string &   filename() const { return _filename; };

    const ErrorVector &     errors() const { return _errors;   };

    void ignore(const std::string &);

    virtual bool obtain();
    virtual bool provide();

 protected:
    virtual bool select(Section **, const std::string & str = "");
    virtual bool adjust(Section *, const std::string & opt, const std::string & val);

    virtual bool deserialize(std::ifstream &);
    virtual bool serialize(std::ofstream &);

    void recurse(std::ofstream &, Section *);

 protected:
    bool         _good;
    ErrorVector  _errors;
    NameSet      _ignores;
    std::string  _filename;
};

#endif /* _CONFIG_CONFIGFILE_HPP_ */
