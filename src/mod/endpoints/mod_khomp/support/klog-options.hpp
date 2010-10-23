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

#include <types.hpp>

#include <configurator/option.hpp>
#include <configurator/section.hpp>

#ifndef _KLOG_OPTIONS_HPP_
#define _KLOG_OPTIONS_HPP_

struct LogOptions: public Section
{
    struct KLogger:   public Section { KLogger();   };
    struct K3L:       public Section { K3L();       };
    struct IntfK3L:   public Section { IntfK3L();   };
    struct IntfK3L_C: public Section { IntfK3L_C(); };
    struct ISDN:      public Section { ISDN();      };
    struct R2:        public Section { R2();        };
    struct Firmware:  public Section { Firmware();  };
    struct Audio:     public Section { Audio();     };
    struct SS7:       public Section { SS7();       };
    struct SIP:       public Section { SIP();       };
    struct GSM:       public Section { GSM();     };
    struct Timer:     public Section { Timer();     };

    LogOptions();

    KLogger     _klogger;
    K3L         _k3l;
    IntfK3L     _intfk3l;
    IntfK3L_C   _intfk3lc;
    ISDN        _isdn;
    R2          _r2;
    Firmware    _firmware;
    Audio       _audio;
    SS7         _ss7;
    SIP         _sip;
    GSM         _gsm;
    Timer       _timer;
};

#endif /* _KLOG_OPTIONS_HPP_ */
