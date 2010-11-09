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

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>

#include <configurator/option.hpp>

#ifndef _CONFIG_SECTION_HPP_
#define _CONFIG_SECTION_HPP_

struct Section
{
	typedef std::map < std::string, Option >   OptionMap;
	typedef std::vector< Option * >            OptionVector;

	typedef std::map < std::string, Section * > SectionMap;
	typedef std::vector < Section * >           SectionVector;

	struct not_found {}; /* exception */
	
// protected:
	Section(std::string name, std::string desc, bool recursive = true)
	: _name(name), _description(desc), _recursive(recursive) {};

	void add(Option o)
	{
		_options.insert(std::pair<std::string,Option>(o.name(), o));
	};

	void del(std::string name)
	{
		_options.erase(name);
	};

	void add(Section *s)
	{
		_sections.insert(std::pair<std::string,Section*>(s->name(), s));
	};

 public:
	const std::string & name()           { return _name;        };
	const std::string & description()    { return _description; };
	const bool		  & recursive()      { return _recursive;   };

	OptionMap::iterator option_begin()   { return _options.begin(); };
	OptionMap::iterator option_end()     { return _options.end();   };

	SectionMap::iterator section_begin() { return _sections.begin(); };
	SectionMap::iterator section_end()   { return _sections.end();   };

	/**/

	Option  *  option_find(const char *, bool recurse = false);
	Section * section_find(const char *, bool recurse = false);

	Option  *  option_find(std::string &, bool recurse = false);
	Section * section_find(std::string &, bool recurse = false);

	/**/

	void options(OptionVector &);
	void sections(SectionVector &);

	/**/

	template <typename F>
	bool search_and_apply(std::string &key, std::string &value, F f)
	{
		OptionMap::iterator i = _options.find(key);

		if (i != _options.end())
			return f((*i).second);

		if (!_recursive)
			return false;

		return (find_if(_sections.begin(), _sections.end(), f) != _sections.end());
	}

 private:
	struct key_value
	{
		key_value(std::string &k, std::string &v): _k(k), _v(v) {};
		std::string & _k, & _v;
	};

	struct load_section: protected key_value
	{
		load_section(std::string &k, std::string &v): key_value(k,v) {};

		bool operator()(Option &o)                 { return o.load(_v);            };
		bool operator()(SectionMap::value_type &v) { return v.second->load(_k,_v); };
	};

	struct change_section: protected key_value
	{
		change_section(std::string &k, std::string &v): key_value(k,v) {};

		bool operator()(Option &o)                 { return o.change(_v);            };
		bool operator()(SectionMap::value_type &v) { return v.second->change(_k,_v); };
	};

	struct store_section: protected key_value
	{
		store_section(std::string &k, std::string &v): key_value(k,v) {};

		bool operator()(Option &o)                 { return o.store(_v);            };
		bool operator()(SectionMap::value_type &v) { return v.second->store(_k,_v); };
	};

	struct set_section: protected key_value
	{
		set_section(std::string &k, std::string &v): key_value(k,v) {};

		bool operator()(Option &o)                 { return (o.set(_v))[Option::F_ADJUSTED]; };
		bool operator()(SectionMap::value_type &v) { return  v.second->set(_k,_v);           };
	};

	struct get_section: protected key_value
	{
		get_section(std::string &k, std::string &v): key_value(k,v) {};

		bool operator()(Option &o)                 { return o.get(_v);            };
		bool operator()(SectionMap::value_type &v) { return v.second->get(_k,_v); };
	};

	struct modified_section
	{
		bool operator()(OptionMap::value_type  &v) { return v.second.modified();  };
		bool operator()(SectionMap::value_type &v) { return v.second->modified(); };
	};

 public:
	bool load(const char * key, std::string value)
	{
		std::string skey(key);
		return search_and_apply(skey, value, load_section(skey, value));
	}

	bool load(std::string &key, std::string &value)
	{
		return search_and_apply(key, value, load_section(key, value));
	}

	bool change(std::string &key, std::string &value)
	{
		return search_and_apply(key, value, change_section(key, value));
	}

	bool store(std::string &key, std::string &value)
	{
		return search_and_apply(key, value, store_section(key, value));
	}

	bool set(std::string &key, std::string &value)
	{
		return search_and_apply(key, value, set_section(key, value));
	}

	bool get(std::string &key, std::string &value)
	{
		return search_and_apply(key, value, get_section(key, value));
	}

	bool modified()
	{
		return ((find_if(_options.begin(), _options.end(), modified_section()) != _options.end()) ||
		        (find_if(_sections.begin(), _sections.end(), modified_section()) != _sections.end()));
	}

 private:
	Section() {};

 protected:
    std::string       _name;
	std::string       _description;

    OptionMap         _options;
    SectionMap        _sections;

    bool              _recursive;
};

#endif /* _CONFIG_SECTION_HPP_ */
