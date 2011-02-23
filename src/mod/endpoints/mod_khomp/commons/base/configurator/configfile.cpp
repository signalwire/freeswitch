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
#include <errno.h>

#include <configurator/configfile.hpp>

#if _MSC_VER >= 1400
#undef close
#endif

void Configfile::ignore(const std::string & str)
{
    _ignores.insert(str);
};

bool Configfile::select(Section **ptr, const std::string & str)
{
    /* default section == this! */
    *ptr = this;

    /* always success by default */
    return true;
};

bool Configfile::adjust(Section * section, const std::string & opt, const std::string & val)
{
    return section->load(opt, val);
};

bool Configfile::deserialize(std::ifstream & fd)
{
    Section * section = NULL;

    /* default selection! */
    if (!select(&section))
    {
        _errors.push_back("default selection has failed!");
        return false;
    }

    size_t count = 0;

    while (fd.good())
    {
        std::string str;

        /* read one line! */
        std::getline(fd, str);

        size_t lst = str.size() - 1;

        if (str.size() >= 1 && str[lst] == '\r') //cuida das quebras de linha do tipo \r\n
        {
            str.erase(lst,1);
            --lst;
        }

        /* empty line! */
        if (str.size() == 0)
            continue;

        /* comment! */
        if (str[0] == '#')
            continue;

        ++count;

        if (str[0] == '[' && str[lst] == ']')
        {
            str.erase(0,1);   --lst;
            str.erase(lst,1); --lst;

            if (!select(&section, str))
            {
                _errors.push_back(STG(FMT("erroneous section '%s'") % str));

                /* ignore this section */
                section = NULL;
                continue;
            }
        }
        else
        {
            std::string::size_type pos = str.find('=');

            if (pos == std::string::npos)
            {
                _errors.push_back(STG(FMT("erroneous separator '%s'") % str));
                continue;
            };

            if (section == NULL)
            {
                _errors.push_back(STG(FMT("no section for option '%s'") % str));
                continue;
            }

            std::string  opt(str.substr(0,pos));
            std::string  val(str.substr(pos+1));

            if (_ignores.find(opt) != _ignores.end())
                continue;

            if (val == "@") val = "";

            if (adjust(section, opt, val))
                continue;

            _errors.push_back(STG(FMT("option '%s' does not exist or '%s' is not "
                "a valid value (at section '%s')") % opt % val % section->name()));
        }
    }

    // retorna 'true' se arquivo tinha alguma coisa valida.
    return (count != 0);
}

bool Configfile::obtain()
{
    std::ifstream fd(_filename.c_str());

    if (!fd.is_open())
    {
        _errors.push_back(STG(FMT("unable to open file '%s': %s")
            % _filename % strerror(errno)));
        return false;
    };

    if (!deserialize(fd))
    {
        fd.close();
        return false;
    }

    fd.close();
    return true;
};

void Configfile::recurse(std::ofstream & fd, Section * section)
{
    typedef Section::SectionMap::const_iterator SectionIter;
    typedef Section::OptionMap::const_iterator  OptionIter;

    for (OptionIter i = section->option_begin(); i != section->option_end(); i++)
    {
        std::string res;

        if ((*i).second.store(res))
        {
            if (res == "") res = "@";
            fd << (*i).first << "=" << res << std::endl;
        }
    }

    if (!section->recursive())
        return;

    for (SectionIter j = section->section_begin(); j != section->section_end(); j++)
        recurse(fd, (*j).second);
}

bool Configfile::serialize(std::ofstream & fd)
{
    recurse(fd, this);
    return true;
}

bool Configfile::provide()
{
    std::string tmp(_filename);
    tmp += ".new";

    std::ofstream fd(tmp.c_str());

    if (!fd.good())
    {
        _errors.push_back(STG(FMT("unable to open file '%s': %s")
            % tmp % strerror(errno)));
        return false;
    }

    if (!serialize(fd))
    {
        fd.close();
        return false;
    }

    fd.close();

    if (rename(tmp.c_str(), _filename.c_str()) != 0)
    {
        _errors.push_back(STG(FMT("unable to replace config file '%s': %s")
            % _filename % strerror(errno)));
        return false;
    }

    return true;
}

#if _MSC_VER >= 1400
#define close _close
#endif
