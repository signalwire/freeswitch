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

#include <stdarg.h>

#include <string>
#include <list>
#include <vector>
#include <map>

#include <const_this.hpp>

#ifndef _CONFIG_RESTRICTION_HPP_
#define _CONFIG_RESTRICTION_HPP_

struct Restriction: public ConstThis < Restriction >
{
    /* generic types */

    // TODO: change this type name for something different
    //       to avoid conflicting with "format.hpp".
    enum Format
    {
        F_USER,
        F_FILE
    };

    enum Kind
    {
        K_STRING,
        K_NUMBER // = K_INTEGER // compatibility
    };

    enum Bounds
    {
        B_FREE,
        B_RANGE,
        B_LIST,
        B_MAPS
    };

    enum Numeral
    {
        N_UNIQUE,
        N_MULTIPLE
    };

    typedef std::string   Value;

    /* types used for data input */
    struct Pair
    {
        const char *pretty;
        const char *value;
    };

    typedef std::pair < Value, Value >   PairMap;
    typedef std::list < PairMap >        ListMap;

    /* types used internally */
    typedef std::map < Value, Value >           Map;
    typedef std::vector < Value >            Vector;

    typedef std::list < Value >                List;
    typedef std::pair < Value, Value >       MapPair;

    struct Generic
    {
        Value _unique;
        List  _multiple;
    };

    Restriction(Kind kind, Numeral num)
    : _kind(kind), _bounds(B_FREE), _numeral(num), _unit(""),
      _init(-1), _fini(-1), _step(-1)
    {
        init_class();
    }

    Restriction(Kind kind, Numeral num,
                double init, double fini, double step = 1)
    : _kind(kind), _bounds(B_RANGE), _numeral(num), _unit(""),
      _init(init), _fini(fini), _step(step)
    {
        init_class();
    }

    Restriction(Kind kind, Numeral num,
                const char *unit, double init, double fini, double step = 1.0)
    : _kind(kind), _bounds(B_RANGE), _numeral(num), _unit(unit),
      _init(init), _fini(fini), _step(step)
    {
        init_class();
    }

    Restriction(Kind kind, Numeral num,
                std::string unit, double init, double fini, double step = 1.0)
    : _kind(kind), _bounds(B_RANGE), _numeral(num), _unit(unit),
      _init(init), _fini(fini), _step(step)
    {
        init_class();
    }

    Restriction(Kind kind, Numeral num,
                const char *first, ...)
    : _kind(kind), _bounds(B_LIST), _numeral(num), _unit(""),
      _init(-1), _fini(-1), _step(-1)
    {
        _list.push_back(std::string(first));

        va_list ap;
        va_start(ap, first);

        while (true)
        {
            const char *arg = va_arg(ap, const char *);

            if (arg == NULL) break;

            _list.push_back(std::string(arg));
        }

        init_class();
    }

    Restriction(Kind kind, const char *unit, Numeral num,
                const char *first, ...)
    : _kind(kind), _bounds(B_LIST), _numeral(num), _unit(unit),
      _init(-1), _fini(-1), _step(-1)
    {
        _list.push_back(std::string(first));

        va_list ap;
        va_start(ap, first);

        while (true)
        {
            const char *arg = va_arg(ap, const char *);

            if (arg == NULL) break;

            _list.push_back(std::string(arg));
        }

        init_class();
    }

    Restriction(Kind kind, Numeral num,
                const struct Pair first, ...)
    : _kind(kind), _bounds(B_MAPS), _numeral(num), _unit(""),
      _init(-1), _fini(-1), _step(-1)
    {
        _map_from_usr.insert(MapPair(Value(first.pretty), Value(first.value)));
        _map_from_cfg.insert(MapPair(Value(first.value), Value(first.pretty)));

        va_list ap;
        va_start(ap, first);

        while (true)
        {
            Pair arg = va_arg(ap, Pair);

            if (arg.pretty == NULL) break;

            _map_from_usr.insert(MapPair(Value(arg.pretty), Value(arg.value)));
            _map_from_cfg.insert(MapPair(Value(arg.value), Value(arg.pretty)));
        }

        init_class();
    }

    Restriction(Kind kind, Numeral num, List list)
    : _kind(kind), _bounds(B_LIST), _numeral(num), _unit(""),
      _init(-1), _fini(-1), _step(-1), _list(list)
    {
        init_class();
    }

    Restriction(Kind kind, Numeral num, ListMap map)
    : _kind(kind), _bounds(B_MAPS), _numeral(num), _unit(""),
      _init(-1), _fini(-1), _step(-1)
    {
        for (ListMap::iterator i = map.begin(); i != map.end(); i++)
        {
            _map_from_usr.insert(MapPair(Value((*i).first), Value((*i).second)));
            _map_from_cfg.insert(MapPair(Value((*i).second), Value((*i).first)));
        }

        init_class();
    }

    const Kind          kind() const { return _kind;    };
    const Bounds      bounds() const { return _bounds;  };
    const Numeral    numeral() const { return _numeral; };

    const std::string & unit() const { return _unit;    };

    bool  set(Format, const Vector &);
    bool  set(Format, const  Value &);

    bool  get(Format, Vector &) const;
    bool  get(Format,  Value &) const;

    void  allowed(Vector &) const;

 private:
    bool    process(const Format, const Value &, Value &) const;
    bool  unprocess(const Format, const Value &, Value &) const;

    void  init_class();

    static bool equalNumber(const double, const double);

 protected:
    const Kind      _kind;
    const Bounds    _bounds;
    const Numeral   _numeral;

    Value     _unit;

    const double    _init, _fini, _step;

    Map       _map_from_usr,
              _map_from_cfg;

    List      _list;

    Generic   _value;
};

#endif /* _CONFIG_RESTRICTION_HPP_ */
