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

#include <configurator/section.hpp>

void Section::options(Section::OptionVector & vec) const
{
    for (OptionMap::const_iterator it = _options.begin(); it != _options.end();)
    {
        vec.push_back(const_cast< Option * >(&(it->second)));
        ++it;
    }
}

void Section::sections(Section::SectionVector & vec) const
{
    for (SectionMap::const_iterator it = _sections.begin(); it != _sections.end();)
    {
        vec.push_back(const_cast< Section * >(it->second));
        ++it;
    }
}

/*********/

Option * Section::option_find(const std::string & str, bool recurse) const
{
    OptionMap::const_iterator i = _options.find(str);

    if (i == _options.end())
    {
        if (!recurse)
            throw OptionNotFound(str, _name);
//            throw not_found();

        for (SectionMap::const_iterator i = _sections.begin(); i != _sections.end(); i++)
        {
            try
            {
                return i->second->option_find(str, recurse);
            }
            catch (NotFound & e)
            {
                /* keep looping! */
            };
        }

//        throw not_found();
        throw OptionNotFound(str, _name);
    }

    return const_cast< Option * >(&(i->second));
}

/*
Option * Section::option_find(const char * str, bool recurse)
{
    std::string sstr(str);
    return option_find(sstr, recurse);
}
*/

/*********/

Section * Section::section_find(const std::string & str, bool recurse) const
{
    SectionMap::const_iterator i = _sections.find(str);

    if (i == _sections.end())
    {
        if (!recurse)
            throw SectionNotFound(str, _name);

        for (SectionMap::const_iterator i = _sections.begin(); i != _sections.end(); i++)
        {
            try
            {
                return i->second->section_find(str, recurse);
            }
            catch (NotFound & e)
            {
                /* keep looping! */
            };
        }

        throw SectionNotFound(str, _name);
    }

    return const_cast< Section * >(i->second);
}

/*
Section * Section::section_find(const char * str, bool recurse)
{
    std::string sstr(str);
    return section_find(sstr, recurse);
}
*/
