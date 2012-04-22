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

#include <support/klog-options.hpp>

LogOptions::KLogger::KLogger()
: Section("KLogger", "KLogger", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("FullLog", "Log everything", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::K3L::K3L()
: Section("K3L", "K3L", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("CallProgress", "Call Progress events", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("CallAnalyzer", "Call Analyzer events", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("CadenceRecog", "Cadences recognized", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("CallControl",  "Call control", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("Fax",          "Fax stack", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::IntfK3L::IntfK3L()
: Section("IntfK3L", "IntfK3L", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("AudioEvent", "Audio events", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::IntfK3L_C::IntfK3L_C()
: Section("IntfK3L_C", "IntfK3L_C", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("AudioEvent", "Audio events", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::ISDN::ISDN()
: Section("ISDN", "ISDN", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("Lapd", "LAPD layer log", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("Q931", "Q.931 layer log", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::R2::R2()
: Section("R2", "R2", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("Signaling", "R2 Signaling log", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("States", "R2 States Log", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::Firmware::Firmware()
: Section("Firmware", "Firmware", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("FwHdlcMsg", "HDLC messages", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("FwLinkErrors", "Link errors", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("FwModemChar", "MODEM char TX/RX", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::Audio::Audio()
: Section("Audio", "Audio", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("DSP", "DSP audio messages", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("K3L", "K3L audio messages", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::SS7::SS7()
: Section("SS7", "SS7", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("MTP2States", "MTP2 States", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("MTP2Debug", "MTP2 Debug", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("MTP3Management", "MTP3 Management", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("MTP3Test", "MTP3 Test", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("MTP3Debug", "MTP3 Debug", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("ISUPStates", "ISUP States", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("ISUPDebug", "ISUP Debug", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
    add(Option("ISUPMessages", "ISUP Messages", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::SIP::SIP()
: Section("SIP", "SIP", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::GSM::GSM()
: Section("GSM", "GSM", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::Timer::Timer()
: Section("Timer", "Timer", false)
{
    add(Option("Value", "Enable log class?", BOOLEAN_FALSE, Restriction(K(STRING), N(UNIQUE), BOOLEAN_VALUE)));
};

LogOptions::LogOptions()
: Section("Options", "KLog Options", false)
{
    add(&_klogger);
    add(&_k3l);
    add(&_intfk3l);
    add(&_intfk3lc);
    add(&_isdn);
    add(&_r2);
    add(&_firmware);
    add(&_audio);
    add(&_ss7);
    add(&_sip);
    add(&_gsm);
    add(&_timer);
};
