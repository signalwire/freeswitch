#!/bin/sh

cut_defines ()
{
    cut -b9- | cut -f1 | cut -d' ' -f1
}

list_commands ()
{
    egrep -h 'define.+CM_' "$@" | cut_defines
}

list_events ()
{
    # list and remove deprecations
    egrep -h 'define.+EV_' "$@" | cut_defines | \
        grep -v 'EV_CALL_PROGRESS\|EV_FAX_MESSAGE_CONFIRMATION'
}

make_enumeration_one_by_one ()
{
    while read line
    do
        local size=$[50 - $(expr length "${line}")]

        echo -n "        K_${line}"

        for ((i=0;i<size;i++))
        do
            echo -n " "
        done

        echo "= ${line},"
    done
}

make_enumeration ()
{
    local name="$1"; shift
    local func="$1"; shift

    echo "    typedef enum"
    echo "    {"
    "${func}" "$@" | make_enumeration_one_by_one
    echo "    }"
    echo "    ${name};"
}

make_switch_case_one_by_one ()
{
    while read line
    do
        local size=$[50 - $(expr length "${line}")]

        echo -n "        case K_${line}:"

        for ((i=0;i<size;i++))
        do
            echo -n " "
        done

        echo "return \"${line}\";"
    done
}

make_switch_case ()
{
    local type="$1"; shift
    local name="$1"; shift
    local func="$1"; shift

    echo "std::string VerboseTraits::${name}Name(const ${type} value)"
    echo "{"
    echo "    switch(value)"
    echo "    {"
    "${func}" "$@" | make_switch_case_one_by_one
    echo "    }"
    echo "    return STG(FMT(\"${name}=%d\") % ((int)value));"
    echo "}"
}

make_license ()
{
    echo '/*'
    echo '    KHOMP generic endpoint/channel library.'
    echo '    Copyright (C) 2007-2010 Khomp Ind. & Com.'
    echo ''
    echo '  The contents of this file are subject to the Mozilla Public License Version 1.1'
    echo '  (the "License"); you may not use this file except in compliance with the'
    echo '  License. You may obtain a copy of the License at http://www.mozilla.org/MPL/'
    echo ''
    echo '  Software distributed under the License is distributed on an "AS IS" basis,'
    echo '  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for'
    echo '  the specific language governing rights and limitations under the License.'
    echo ''
    echo '  Alternatively, the contents of this file may be used under the terms of the'
    echo '  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which'
    echo '  case the provisions of "LGPL License" are applicable instead of those above.'
    echo ''
    echo '  If you wish to allow use of your version of this file only under the terms of'
    echo '  the LGPL License and not to allow others to use your version of this file under'
    echo '  the MPL, indicate your decision by deleting the provisions above and replace them'
    echo '  with the notice and other provisions required by the LGPL License. If you do not'
    echo '  delete the provisions above, a recipient may use your version of this file under'
    echo '  either the MPL or the LGPL License.'
    echo ''
    echo '  The LGPL header follows below:'
    echo ''
    echo '    This library is free software; you can redistribute it and/or'
    echo '    modify it under the terms of the GNU Lesser General Public'
    echo '    License as published by the Free Software Foundation; either'
    echo '    version 2.1 of the License, or (at your option) any later version.'
    echo ''
    echo '    This library is distributed in the hope that it will be useful,'
    echo '    but WITHOUT ANY WARRANTY; without even the implied warranty of'
    echo '    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU'
    echo '    Lesser General Public License for more details.'
    echo ''
    echo '    You should have received a copy of the GNU Lesser General Public License'
    echo '    along with this library; if not, write to the Free Software Foundation, Inc.,'
    echo '    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA'
    echo ''
    echo '*/'

    echo "/* ****************************************************************************** */"
    echo "/* ******************* AUTO GENERATED FILE - DO NOT EDIT! *********************** */"
    echo "/* ****************************************************************************** */"
    echo
}

make_header ()
{
    make_license

    echo "#ifndef _VERBOSE_TRAITS_H_"
    echo "#define _VERBOSE_TRAITS_H_"
    echo
    echo "#include <string>"
    echo
    echo "#include <format.hpp>"
    echo
    echo "#include <k3l.h>"
    echo
    echo "struct VerboseTraits"
    echo "{"
    make_enumeration "Command" list_commands "$@" || return 1
    echo
    make_enumeration "Event"   list_events   "$@" || return 1
    echo
    echo "    static std::string   eventName(const Event);"
    echo "    static std::string commandName(const Command);"
    echo "};"
    echo
    echo "#endif /* _VERBOSE_TRAITS_H_ */"
}

make_source ()
{
    make_license

    echo "#include <verbose_traits.hpp>"
    echo

    make_switch_case "Event"   "event"   list_events   "$@" || return 1
    echo
    make_switch_case "Command" "command" list_commands "$@" || return 1
}

make_run ()
{
    local destdir="$1"; shift

    if [ ! -d "${destdir}" ]
    then
        echo "ERROR: First argument is not a directory!"
        return 1
    fi

    make_header "$@" > "${destdir}/verbose_traits.hpp"
    make_source "$@" > "${destdir}/verbose_traits.cpp"
}

make_run "$@"
