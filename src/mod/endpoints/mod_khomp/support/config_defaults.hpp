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

#include <configurator/restriction.hpp>

#ifndef _CONFIG_DEFAULTS_HPP_
#define _CONFIG_DEFAULTS_HPP_

/* default khomp config directory */
#define KHOMP_CONFIG_DIR "/etc/khomp/config"

/* define util macro for text conversion */
#define CONV(s) \
    encoder::recode_env(encoder::C_ISO88591, s)

/* define another util macro for text conversion */
#define CONVU(s) \
    encoder::recode_env(encoder::C_UTF8, s)

/* shortcut */
#define K(x) Restriction::K_##x
#define N(x) Restriction::N_##x
#define B(x) Restriction::B_##x

/* boolean value */
extern Restriction::Pair booleanValue[3];

/* nasty shortcuts */
#define BOOLEAN_VALUE        booleanValue[0], booleanValue[1], booleanValue[2]

#define BOOLEAN_ALWAYS_TRUE  booleanValue[0], booleanValue[2]
#define BOOLEAN_ALWAYS_FALSE booleanValue[1], booleanValue[2]

#define BOOLEAN_TRUE         booleanValue[0].pretty
#define BOOLEAN_FALSE        booleanValue[1].pretty

#endif /* _CONFIG_DEFAULTS_HPP_ */
