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

#ifndef _CONFIG_OPTIONS_HPP_
#define _CONFIG_OPTIONS_HPP_

#include <set>
#include <map>
#include <vector>
#include <stdexcept>

#include <strings.hpp>
#include <format.hpp>
#include <tagged_union.hpp>
#include <function.hpp>
#include <variable.hpp>

namespace Config
{
    /* exceptions */

    struct Failure: public std::runtime_error
    {
        Failure(const std::string & msg) : std::runtime_error(msg) {};
    };

    struct EmptyValue: public std::runtime_error
    {
        EmptyValue(): std::runtime_error("accessed option still not loaded from configuration") {};
    };

    /* types */

    typedef int          SIntType;
    typedef unsigned int UIntType;
    typedef bool         BooleanType;
    typedef std::string  StringType;

    template < typename Type >
    struct Value;

    template < typename Type >
    struct InnerOptionBase;

    template < typename Type >
    struct InnerOption;

    struct Option;

    /* here we go! */

    template < typename Type >
    struct Range
    {
        Range(const Type _minimum, const Type _maximum, const Type _step)
        : minimum(_minimum), maximum(_maximum), step(_step) {};

        const Type minimum, maximum, step;
    };

    typedef std::set < std::string > StringSet;

    template < typename Type >
    struct Value: COUNTER_SUPER(Value < Type >)
    {
        friend class COUNTER_CLASS(Value < Type >);
        friend class InnerOptionBase< Type >;
        friend class InnerOption < Type >;
        friend class Option;

        Value()
        : _tmpval(0), _stored(0), _loaded(false), _inited(false)
        {};

        Value(const Value & o)
        : COUNTER_REFER(o, Value < Type >),
          _tmpval(o._tmpval), _stored(o._stored),
          _loaded(o._loaded), _inited(o._inited)
        {};

        const Type & operator()(void) const
        {
            if (!_inited)
                throw EmptyValue();

            if (!_stored)
                return *_tmpval;

            return *_stored;
        };

        const Type &     get(void) const { return operator()(); };
        bool          loaded(void) const { return _loaded;      };

        void store(const Type val)
        {
            if (_tmpval)
            {
                delete _tmpval;
                _tmpval = 0;
            }

            _tmpval = new Type(val);

            _loaded = true;
            _inited = true;
        }

    protected:
        void unreference(void)
        {
            _inited = false;
            _loaded = false;

            if (_tmpval)
            {
                delete _tmpval;
                _tmpval = 0;
            }

            if (_stored)
            {
                delete _stored;
                _stored = 0;
            }
        };

    protected:
        void commit(Type def)
        {
            if (_tmpval)
            {
                {
                    delete _stored;
                    _stored = 0;
                }

                _stored = _tmpval;
                _tmpval = 0;
            }
            else
            {
                if (!_stored)
                    _stored = new Type(def);
            }

            _loaded = true;
            _inited = true;
        };

        void reset(void)
        {
            _loaded = false;
        }

    protected:
        const Type * _tmpval;
        const Type * _stored;
        bool _loaded;
        bool _inited;
    };

    struct FunctionValue
    {
        friend class InnerFunctionType;
        friend class Option;

        FunctionValue()
        : _loaded(false), _inited(false) {};

        virtual ~FunctionValue() {};

    public:
        virtual void operator()(const StringType & val)
        {
            throw Failure("undefined operator() for value");
        }

        const StringType &     get(void) const
        {
            if (!_inited)
                throw EmptyValue();

            return _stored;
        };

        bool loaded(void) const { return _loaded; };

    protected:
        void commit(const StringType def)
        {
            if (_tmpval.empty())
            {
                _stored = def;
            }
            else
            {
                _stored = _tmpval;
                _tmpval.clear();
            }

            operator()(_stored);

            _loaded = true;
            _inited = true;
        };

        void store(const StringType val)
        {
            _tmpval = val;
            _loaded = true;
            _inited = true;
        }

        void reset(void)
        {
            _loaded = false;
        }

    private:
        StringType _tmpval;
        StringType _stored;
        bool _loaded;
        bool _inited;
    };

    /* NOTE: we use a non-templated classe to place this functions inside the .cpp */
    struct Restriction
    {
        static void checkRange(const std::string & name,     const SIntType value, const Range < SIntType > & range);
        static void checkRange(const std::string & name,     const UIntType value, const Range < UIntType > & range);
        static void checkStringSet(const std::string & name, const StringType & value, const StringSet & allowed);
    };

    template < typename Type >
    struct InnerOptionBase
    {
        typedef Variable < Value < Type > > MemberValue;

        InnerOptionBase(const std::string name, MemberValue option, const Type defvalue)
        : _name(name), _option(option), _default(defvalue) {};

        template < typename Object >
        void reset(Object * const obj) const
        {
            _option(obj).reset();
        }

        template < typename Object >
        const Type & get(const Object * const obj) const
        {
            return _option(obj).get();
        }

        template < typename Object >
        bool loaded(const Object * const obj) const
        {
            return _option(obj).loaded();
        }

    protected:
        const std::string   _name;
        MemberValue         _option;
        const Type          _default;
    };

    template < >
    struct InnerOption < SIntType >: public InnerOptionBase < SIntType >
    {
        typedef InnerOptionBase < SIntType >  Super;
        typedef Super::MemberValue           MemberValue;

        InnerOption(const std::string name, MemberValue option, const SIntType defval,
                    const SIntType min, const SIntType max, const SIntType step)
        : Super(name, option, defval), _range(min, max, step) {};

        template < typename Object >
        void commit(Object * const obj) const
        {
            Restriction::checkRange(_name, _default, _range);
            _option(obj).commit(_default);
        };

        template < typename Object >
        void store(Object * const obj, const SIntType stored) const
        {
            Restriction::checkRange(_name, _default, _range);
            _option(obj).store(stored);
        }

        using Super::reset;
        using Super::get;

        const Range< SIntType >  _range;
    };

    template < >
    struct InnerOption < UIntType >: public InnerOptionBase < UIntType >
    {
        typedef InnerOptionBase < UIntType >  Super;
        typedef Super::MemberValue                 MemberValue;

        InnerOption(const std::string name, MemberValue option, const UIntType defval,
                    const UIntType min, const UIntType max, const UIntType step)
        : Super(name, option, defval), _range(min, max, step) {};

        template < typename Object >
        void commit(Object * const obj) const
        {
            Restriction::checkRange(_name, _default, _range);
            _option(obj).commit(_default);
        };

        template < typename Object >
        void store(Object * const obj, const UIntType stored) const
        {
            Restriction::checkRange(_name, _default, _range);
            _option(obj).store(stored);
        }

        using Super::reset;
        using Super::get;

        const Range< UIntType >  _range;
    };

    template < >
    struct InnerOption < BooleanType >: public InnerOptionBase < BooleanType >
    {
        typedef InnerOptionBase < BooleanType >  Super;
        typedef Super::MemberValue                    MemberValue;

        InnerOption(std::string name, MemberValue option, BooleanType defval)
        : Super(name, option, defval) {};

        template < typename Object >
        void commit(Object * obj) const
        {
            _option(obj).commit(_default);
        };

        template < typename Object >
        void store(Object * obj, BooleanType stored) const
        {
            _option(obj).store(stored);
        }

        using Super::reset;
        using Super::get;
    };

    template < >
    struct InnerOption < StringType >: public InnerOptionBase < StringType >
    {
        typedef InnerOptionBase < StringType >  Super;
        typedef Super::MemberValue              MemberValue;

        InnerOption(const std::string name, MemberValue option, const StringType defval, const StringSet & allowed)
        : Super(name, option, defval), _allowed(allowed) {};

        InnerOption(const std::string name, MemberValue option, const StringType defval)
        : Super(name, option, defval) {};

        template < typename Object >
        void commit(Object * const obj) const
        {
            Restriction::checkStringSet(_name, _default, _allowed);
            _option(obj).commit(_default);
        };

        template < typename Object >
        void store(Object * const obj, const StringType stored) const
        {
            Restriction::checkStringSet(_name, _default, _allowed);
            _option(obj).store(stored);
        }

        using Super::reset;
        using Super::get;

        const StringSet _allowed;
    };

    struct InnerFunctionType
    {
        typedef Variable < FunctionValue > MemberValue;

        InnerFunctionType(const std::string name, MemberValue option, const StringType defval, const StringSet & allowed)
        : _name(name), _option(option), _default(defval), _allowed(allowed) {};

        InnerFunctionType(const std::string name, MemberValue option, const StringType defval)
        : _name(name), _option(option), _default(defval) {};

        template < typename Object >
        const StringType & get(const Object * const obj) const
        {
            return _option(obj).get();
        }

        template < typename Object >
        bool loaded(const Object * const obj) const
        {
            return _option(obj).loaded();
        }

        template < typename Object >
        void reset(Object * const obj) const
        {
            _option(obj).reset();
        }

        template < typename Object >
        void commit(Object * const obj) const
        {
            Restriction::checkStringSet(_name, _default, _allowed);
            _option(obj).commit(_default);
        };

        template < typename Object >
        void store(Object * const obj, const StringType stored) const
        {
            Restriction::checkStringSet(_name, _default, _allowed);
            _option(obj).store(stored);
        }

    protected:
        const std::string   _name;
        MemberValue         _option;
        const StringType    _default;

    public:
        const StringSet     _allowed;
    };

    struct Option
    {
        typedef InnerOption < SIntType >        InnerSIntType;
        typedef InnerOption < UIntType >        InnerUIntType;
        typedef InnerOption < BooleanType >  InnerBooleanType;
        typedef InnerOption < StringType >    InnerStringType;

        typedef Variable < Value < SIntType > >         SIntMemberType;
        typedef Variable < Value < UIntType > >         UIntMemberType;
        typedef Variable < Value < BooleanType > >   BooleanMemberType;
        typedef Variable < Value < StringType > >     StringMemberType;

        typedef Variable < FunctionValue >          FunctionMemberType;

        typedef Tagged::Union < InnerStringType,
                Tagged::Union < InnerBooleanType,
                Tagged::Union < InnerSIntType ,
                Tagged::Union < InnerUIntType,
                Tagged::Union < InnerFunctionType > > > > >
            InnerType;

        explicit Option(std::string, StringMemberType,   const StringType, StringSet & allowed, bool listme = true);
        explicit Option(std::string, StringMemberType,   const StringType = "", bool listme = true);
        explicit Option(std::string, SIntMemberType,     const SIntType = 0, SIntType min =  INT_MIN, SIntType max =  INT_MAX, SIntType step = 1, bool listme = true);
        explicit Option(std::string, UIntMemberType,     const UIntType = 0, UIntType min = 0, UIntType max = UINT_MAX, UIntType step = 1, bool listme = true);
        explicit Option(std::string, BooleanMemberType,  const BooleanType = false, bool listme = true);

        explicit Option(std::string, FunctionMemberType, const StringType, StringSet & allowed, bool listme = true);
        explicit Option(std::string, FunctionMemberType, const StringType = "", bool listme = true);

        Option(const Option & o);

        ~Option(void);

        template < typename Object >
        void set(Object * object, std::string value)
        {
            try
            {
                /**/ if (_option.check<InnerFunctionType>()) _option.get<InnerFunctionType>().store(object, value);
                else if (_option.check<InnerStringType>())   _option.get<InnerStringType>().store(object, value);
                else if (_option.check<InnerBooleanType>())  _option.get<InnerBooleanType>().store(object, Strings::toboolean(value));
                else if (_option.check<InnerSIntType>())     _option.get<InnerSIntType>().store(object, Strings::tolong(value));
                else if (_option.check<InnerUIntType>())     _option.get<InnerUIntType>().store(object, Strings::toulong(value));
                else
                {
                    throw Failure(STG(FMT("set() not implemented for type used in option '%s'") % _myname));
                }
            }
            catch (Strings::invalid_value & e)
            {
                throw Failure(STG(FMT("got invalid value '%s' for option '%s'") % value % _myname));
            }
            catch (EmptyVariable & e)
            {
                throw Failure(STG(FMT("uninitialized variable while setting value '%s' for option '%s'") % value % _myname));
            }
        }

        template < typename Object >
        std::string get(const Object * const object) const
        {
            try
            {
                /**/ if (_option.check<InnerFunctionType>()) return _option.get<InnerFunctionType>().get(object);
                else if (_option.check<InnerStringType>())   return _option.get<InnerStringType>().get(object);
                else if (_option.check<InnerBooleanType>())  return (_option.get<InnerBooleanType>().get(object) ? "yes" : "no");
                else if (_option.check<InnerSIntType>())     return STG(FMT("%d") % _option.get<InnerSIntType>().get(object));
                else if (_option.check<InnerUIntType>())     return STG(FMT("%u") % _option.get<InnerUIntType>().get(object));
                else
                {
                    throw Failure(STG(FMT("get() not implemented for type used in option '%s'") % _myname));
                }
            }
            catch (EmptyVariable & e)
            {
                throw Failure(STG(FMT("uninitialized variable while getting value for option '%s'") % _myname));
            }
        }

        template < typename Object >
        bool loaded(const Object * const object) const
        {
            try
            {
                /**/ if (_option.check<InnerFunctionType>()) return _option.get<InnerFunctionType>().loaded(object);
                else if (_option.check<InnerBooleanType>())  return _option.get<InnerBooleanType>().loaded(object);
                else if (_option.check<InnerStringType>())   return _option.get<InnerStringType>().loaded(object);
                else if (_option.check<InnerSIntType>())     return _option.get<InnerSIntType>().loaded(object);
                else if (_option.check<InnerUIntType>())     return _option.get<InnerUIntType>().loaded(object);
                else
                {
                    throw Failure(STG(FMT("loaded() not implemented for type used in option '%s'") % _myname));
                }
            }
            catch (EmptyVariable & e)
            {
                throw Failure(STG(FMT("uninitialized variable while checking load status for option '%s'") % _myname));
            }
        }

        template < typename Object >
        void reset(Object * const object)
        {
            try
            {
                /**/ if (_option.check<InnerFunctionType>()) _option.get<InnerFunctionType>().reset(object);
                else if (_option.check<InnerBooleanType>())  _option.get<InnerBooleanType>().reset(object);
                else if (_option.check<InnerStringType>())   _option.get<InnerStringType>().reset(object);
                else if (_option.check<InnerSIntType>())     _option.get<InnerSIntType>().reset(object);
                else if (_option.check<InnerUIntType>())     _option.get<InnerUIntType>().reset(object);
                else
                {
                    throw Failure(STG(FMT("reset() not implemented for type used in option '%s'") % _myname));
                }
            }
            catch (EmptyVariable & e)
            {
                throw Failure(STG(FMT("uninitialized variable while reseting status for option '%s'") % _myname));
            }
        }

        template < typename Object >
        void commit(Object * const object)
        {
            try
            {
                /**/ if (_option.check<InnerFunctionType>()) _option.get<InnerFunctionType>().commit(object);
                else if (_option.check<InnerBooleanType>())  _option.get<InnerBooleanType>().commit(object);
                else if (_option.check<InnerStringType>())   _option.get<InnerStringType>().commit(object);
                else if (_option.check<InnerSIntType>())     _option.get<InnerSIntType>().commit(object);
                else if (_option.check<InnerUIntType>())     _option.get<InnerUIntType>().commit(object);
                else
                {
                    throw Failure(STG(FMT("commit() not implemented for type used in option '%s'") % _myname));
                }
            }
            catch (EmptyVariable & e)
            {
                throw Failure(STG(FMT("uninitialized variable while commiting option '%s'") % _myname));
            }
        }

        const std::string &   name(void) const { return _myname; }
        bool                listme(void) const { return _listme; };

        const char **       values(void);

        template < typename Object >
        void                copyFrom(const Object * const srcobj, Object * const dstobj, bool force = false)
        {
            if (loaded(dstobj) && !force)
                return;

            if (loaded(srcobj))
                set(dstobj, get(srcobj));
            else
                reset(dstobj);
        }

    protected:
        const std::string  _myname;
        InnerType          _option;
        const bool         _listme;
        const char **      _values;
    };

    struct Options
    {
        typedef std::vector < std::string >    Messages;

        Options();
        ~Options();

        typedef std::set < std::string >  StringSet;

        typedef std::map  < std::string, Option >   OptionMap;
        typedef std::pair < std::string, Option >  OptionPair;

        typedef std::map  < std::string, std::string >   SynOptionMap;
        typedef std::pair < std::string, std::string >  SynOptionPair;

        bool add(Option option);

        /* only valid in "process" (for backwards compatibility config files) */
        bool synonym(std::string, std::string);

        template < typename Type >
        void set(const std::string & name, Type value)
        {
            OptionMap::iterator iter = find_option(name);

            if (iter == _map.end())
                throw Failure(STG(FMT("unknown option: %s") % name));

            iter->second.set(value);
        }

        template < typename Object >
        std::string get(const Object * const object, const std::string & name)
        {
            OptionMap::iterator iter = find_option(name);

            if (iter == _map.end())
                throw Failure(STG(FMT("unknown option: %s") % name));

            return iter->second.get(object);
        }

        template < typename Object >
        void process(Object * const object, const char * name, const char * value)
        {
            OptionMap::iterator iter = find_option(name);

            if (iter == _map.end())
                throw Failure(STG(FMT("unknown option '%s'") % name));

            iter->second.set(object, value);
        }

        template < typename Object >
        Messages commit(Object * const object, const std::string & name)
        {
            Messages msgs;

            OptionMap::iterator i = _map.find(name);

            if (i != _map.end())
            {
                try
                {
                    i->second.commit(object);
                }
                catch (Failure & e)
                {
                    msgs.push_back(e.what());
                }
            }
            else
            {
                msgs.push_back(STG(FMT("unable to find option: %s") % name));
            };

            return msgs;
        }

        template < typename Object >
        Messages commit(Object * const object)
        {
            Messages msgs;

            for (OptionMap::iterator i = _map.begin(); i != _map.end(); ++i)
            {
                try
                {
                    i->second.commit(object);
                }
                catch (Failure & e)
                {
                    msgs.push_back(e.what());
                }
            }

            return msgs;
        }

        template < typename Object >
        void reset(Object * object)
        {
            for (OptionMap::iterator i = _map.begin(); i != _map.end(); ++i)
                i->second.reset(object);
        }

        template < typename Object >
        bool loaded(Object * object, const std::string & name)
        {
            OptionMap::iterator iter = find_option(name);

            if (iter == _map.end())
                return false;

            return iter->second.loaded(object);
        }

        bool exists(const std::string & name)
        {
            OptionMap::iterator iter = find_option(name);

            return (iter != _map.end());
        }

        StringSet options(void);

        const char ** values(const char *); /* option value */
        const char ** values(void);         /* values from options */

        template < typename Object >
        void copyFrom(const std::string & name, const Object * const src_obj, Object * const dst_obj, bool force = false)
        {
            OptionMap::iterator iter = find_option(name);

            if (iter == _map.end())
                throw Failure(STG(FMT("unknown option '%s'") % name));

            iter->second.copyFrom(src_obj, dst_obj, force);
        }

        template < typename Object >
        void copyFrom(Object * src_obj, Object * dst_obj, bool force = false)
        {
            for (OptionMap::iterator iter = _map.begin(); iter != _map.end(); ++iter)
                iter->second.copyFrom(src_obj, dst_obj, force);
        }

    protected:
        OptionMap::iterator find_option(std::string);

    protected:
        OptionMap      _map;
        SynOptionMap   _syn_map;

        const char **  _values;
    };
};

#endif /* _CONFIG_OPTIONS_HPP_ */
