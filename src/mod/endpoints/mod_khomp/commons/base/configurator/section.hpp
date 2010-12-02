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

#ifndef _CONFIG_SECTION_HPP_
#define _CONFIG_SECTION_HPP_

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>

#include <format.hpp>

#include <configurator/option.hpp>

struct Section
{
    typedef std::map < std::string, Option >   OptionMap;
    typedef std::vector< Option * >            OptionVector;

    typedef std::map < std::string, Section * > SectionMap;
    typedef std::vector < Section * >           SectionVector;

    struct NotFound: public std::runtime_error
    {
        NotFound(const std::string & type, const std::string & name, const std::string & me)
        : std::runtime_error(STG(FMT("%s '%s' not found on section '%s'") % type % name % me)) {};
    };

    struct OptionNotFound: public NotFound
    {
        OptionNotFound(const std::string & name, const std::string & me)
        : NotFound("option", name, me) {};
    };

    struct SectionNotFound: public NotFound
    {
        SectionNotFound(const std::string & name, const std::string & me)
        : NotFound("section", name, me) {};
    };

    typedef NotFound not_found; /* backward compatibility */

// protected:
    Section(const std::string & name, const std::string & desc, bool recursive = true)
    : _name(name), _description(desc), _recursive(recursive) {};

    void add(const Option & o)
    {
        _options.insert(std::pair<std::string,Option>(o.name(), o));
    };

    void del(const std::string & name)
    {
        _options.erase(name);
    };

    void add(Section * s)
    {
        _sections.insert(std::pair< std::string, Section * >(s->name(), s));
    };

 public:
    const std::string & name()        const { return _name;        };
    const std::string & description() const { return _description; };

    const bool          recursive()   const { return _recursive;   };

    OptionMap::const_iterator option_begin()   const { return _options.begin(); };
    OptionMap::const_iterator option_end()     const { return _options.end();   };

    SectionMap::const_iterator section_begin() const { return _sections.begin(); };
    SectionMap::const_iterator section_end()   const { return _sections.end();   };

    /**/

//    Option  *  option_find(const char *, bool recurse = false) const;
//    Section * section_find(const char *, bool recurse = false) const;

    Option  *  option_find(const std::string &, bool recurse = false) const;
    Section * section_find(const std::string &, bool recurse = false) const;

    /**/

    void  options(OptionVector &)  const;
    void sections(SectionVector &) const;

    /**/

    template < typename T, typename F >
    bool search_and_apply(const std::string & key, T & value, F f)
    {
        OptionMap::iterator i = _options.find(key);

        if (i != _options.end())
            return f(i->second);

        if (!_recursive)
            return false;

        return (find_if(_sections.begin(), _sections.end(), f) != _sections.end());
    }

 private:
    struct ConstKeyValue
    {
        ConstKeyValue(const std::string & k, const std::string &v)
        : _k(k), _v(v) {};

        const std::string & _k;
        const std::string & _v;
    };

    struct KeyValue
    {
        KeyValue(const std::string & k, std::string &v)
        : _k(k), _v(v) {};

        const std::string & _k;
              std::string & _v;
    };

    struct load_section: protected ConstKeyValue
    {
        load_section(const std::string & k, const std::string & v): ConstKeyValue(k,v) {};

        bool operator()(Option & o)                 { return o.load(_v);            };
        bool operator()(SectionMap::value_type & v) { return v.second->load(_k,_v); };
    };

    struct change_section: protected ConstKeyValue
    {
        change_section(const std::string & k, const std::string & v): ConstKeyValue(k,v) {};

        bool operator()(Option & o)                 { return o.change(_v);            };
        bool operator()(SectionMap::value_type & v) { return v.second->change(_k,_v); };
    };

    struct store_section: protected KeyValue
    {
        store_section(const std::string & k, std::string & v): KeyValue(k,v) {};

        bool operator()(Option & o)                 { return o.store(_v);            };
        bool operator()(SectionMap::value_type & v) { return v.second->store(_k,_v); };
    };

    struct set_section: protected ConstKeyValue
    {
        set_section(const std::string & k, const std::string & v): ConstKeyValue(k,v) {};

        bool operator()(Option & o)                 { return (o.set(_v))[Option::F_ADJUSTED]; };
        bool operator()(SectionMap::value_type & v) { return  v.second->set(_k,_v);           };
    };

    struct get_section: protected KeyValue
    {
        get_section(const std::string & k, std::string & v): KeyValue(k,v) {};

        bool operator()(Option & o)                 { return o.get(_v);            };
        bool operator()(SectionMap::value_type & v) { return v.second->get(_k,_v); };
    };

    struct modified_section
    {
        bool operator()(const OptionMap::value_type  & v) { return v.second.modified();  };
        bool operator()(const SectionMap::value_type & v) { return v.second->modified(); };
    };

 public:
/*
    bool load(const char * key, const std::string value)
    {
        std::string skey(key);
        return search_and_apply(skey, value, load_section(skey, value));
    }
*/
    bool load(const std::string & key, const std::string & value)
    {
        return search_and_apply(key, value, load_section(key, value));
    }

    bool change(const std::string & key, const std::string & value)
    {
        return search_and_apply(key, value, change_section(key, value));
    }

    bool store(const std::string & key, std::string & value)
    {
        return search_and_apply(key, value, store_section(key, value));
    }

    bool set(const std::string & key, const std::string & value)
    {
        return search_and_apply(key, value, set_section(key, value));
    }

    bool get(const std::string & key, std::string & value)
    {
        return search_and_apply(key, value, get_section(key, value));
    }

    bool modified() const
    {
        return ((find_if(_options.begin(),  _options.end(),  modified_section()) != _options.end()) ||
                (find_if(_sections.begin(), _sections.end(), modified_section()) != _sections.end()));
    }

 private:
    Section(): _name(""), _description(""), _recursive(false) {};

 protected:
    const std::string   _name;
    const std::string   _description;

    OptionMap           _options;
    SectionMap          _sections;

    const bool          _recursive;
};

#endif /* _CONFIG_SECTION_HPP_ */
