/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

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
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#include "globals.h"

K3LAPI                           Globals::k3lapi;
K3LUtil                          Globals::k3lutil(Globals::k3lapi);
Verbose                          Globals::verbose(Globals::k3lapi);

/* Global Timer */
Globals::GlobalTimer           * Globals::global_timer = NULL;

Globals::Mutex                   Globals::khomp_alloc_mutex;

Config::Options                  Globals::options;

const Regex::Expression          Globals::regex_allocation("(((([bB])[ ]*([0-9]+))|(([sS])[ ]*([0-9]+)))[ ]*(([cClL])[ ]*([0-9]+)[ ]*([-][ ]*([0-9]+))?)?)|(([rR])[ ]*([0-9]+)[ ]*([-][ ]*([0-9]+))?)", Regex::E_EXTENDED);

switch_endpoint_interface_t    * Globals::khomp_endpoint_interface     = NULL;
switch_endpoint_interface_t    * Globals::khomp_sms_endpoint_interface = NULL;
switch_endpoint_interface_t    * Globals::khomp_pr_endpoint_interface  = NULL;
switch_application_interface_t * Globals::khomp_app_inteface           = NULL;
switch_api_interface_t         * Globals::api_interface                = NULL;
switch_memory_pool_t           * Globals::module_pool                  = NULL;

volatile bool                    Globals::logs_being_rotated = false;

bool                             Globals::flag_trace_rdsi = false;
