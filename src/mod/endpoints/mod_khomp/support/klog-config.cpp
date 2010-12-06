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

#include <config_defaults.hpp>

#include <support/klog-config.hpp>

Logfile::Logfile()
: Configfile("Options", "Log Options")
{
    _filename = STG(FMT("%s/klog.cfg") % KHOMP_CONFIG_DIR);

    // necessario?
    add(&_logoptions);

    /* Obtain configuration, if file exists. */
    _good = obtain();
}

Logfile::~Logfile()
{
    /* nothing for now */
};

#if COMMONS_AT_LEAST(1,1)
bool Logfile::select(Section ** ptr, const std::string & str)
#else
bool Logfile::select(Section ** ptr, std::string str)
#endif
{
    // default section, needed for API compliance.
    if (str == "")
        *ptr = &_logoptions;
    else
    {
        try
        {
            *ptr = _logoptions.section_find(str);
        }
        catch (Section::not_found e)
        {
            _errors.push_back(STG(FMT("unsupported section '%s', ignoring!") % str));
        }
    }

    return (*ptr != 0);
};

bool Logfile::serialize(std::ofstream &fd)
{
    typedef std::vector < std::string > StrList;

    StrList vec;

    vec.push_back("KLogger");
    vec.push_back("K3L");
    vec.push_back("IntfK3L");
    vec.push_back("IntfK3L_C");
    vec.push_back("ISDN");
    vec.push_back("R2");
    vec.push_back("Firmware");
    vec.push_back("Audio");
    vec.push_back("SS7");
    vec.push_back("SIP");
    vec.push_back("GSM");
    vec.push_back("Timer");

    for (StrList::iterator i = vec.begin(); i != vec.end(); i++)
    {
        fd << "[" << (*i) << "]" << std::endl;

        try
        {
            Section * sec = _logoptions.section_find(*i);
            recurse(fd, sec);
        }
        catch (Section::not_found e)
        {
            _errors.push_back(STG(FMT("unable to find section '%s' in memory, ignoring!") % (*i)));
        }

        fd << std::endl;
    }

    fd << "# precisa ter um caracter sobrando no final!" << std::endl;

    return true;
}
