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

#include "khomp_pvt_passive.h"
#include "lock.h"
#include "logger.h"

bool BoardPassive::KhompPvtPassive::validContexts(
        MatchExtension::ContextListType & contexts, std::string extra_context)
{
    DBG(FUNC,PVT_FMT(_target,"(Passive) c"));

    if(!_group_context.empty())
    {
        contexts.push_back(_group_context);
    }

    contexts.push_back(Opt::_options._context_pr());

    for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++)
    {
        replaceTemplate((*i), "CC", _target.object);
    }

    bool ret = Board::KhompPvt::validContexts(contexts,extra_context);

    DBG(FUNC,PVT_FMT(_target,"(Passive) r"));

    return ret;
}

bool BoardPassive::KhompPvtHI::onSeizureStart(K3L_EVENT *e)
{
    DBG(FUNC,PVT_FMT(_target,"(HI) c"));
   
    try
    {
        ScopedPvtLock lock(this);

        _call->_orig_addr.clear();
        _call->_dest_addr = "s";

#if SWITCH_LESS_THAN(1,0,6)
        session(switch_core_session_request(Globals::khomp_pr_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL));
#else
        session(switch_core_session_request(Globals::khomp_pr_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL));
#endif

        if (justAlloc(true) != SWITCH_STATUS_SUCCESS)
        {
            setHangupCause(SWITCH_CAUSE_UNALLOCATED_NUMBER);
            cleanup(CLN_FAIL);
            LOG(ERROR, PVT_FMT(target(), "(HI) r (Initilization Error on alloc!)"));
            return false;
        }

        /* begin context adjusting + processing */
        MatchExtension::ContextListType contexts;

        validContexts(contexts);

        std::string tmp_exten;
        std::string tmp_context;

        switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, call()->_dest_addr, call()->_orig_addr, false))
        {
        case MatchExtension::MATCH_NONE:
            destroy();
            owner(NULL);
            LOG(WARNING, PVT_FMT(_target, "(HI) r (unable to find exten/context on incoming call %s/%s)")
                    % _call->_dest_addr % (contexts.size() >= 1 ? contexts[0] : "default"));
            return false;
        default:
            DBG(FUNC, PVT_FMT(_target, "(HI) our: context '%s', exten '%s'") % tmp_context % tmp_exten);
            break;

        }

        call()->_incoming_context = tmp_context;
        _call->_dest_addr = tmp_exten;

        startListen();

        if (justStart() != SWITCH_STATUS_SUCCESS)
        {
            setHangupCause(SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
            cleanup(CLN_FAIL);
            LOG(ERROR, PVT_FMT(target(), "(HI) r (Initilization Error on start!)"));
            return false;
        }

        call()->_flags.set(Kflags::REALLY_CONNECTED);
        call()->statistics()->incrementNewCall();

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(HI) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(HI) r"));

    return true;
}

bool BoardPassive::KhompPvtKPR::onNewCall(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(KPR) c"));

    try
    {
        std::string orig_addr, dest_addr;

        Globals::k3lapi.get_param(e, "orig_addr", orig_addr);
        Globals::k3lapi.get_param(e, "dest_addr", dest_addr);
        
        if(dest_addr.empty())
        {
            dest_addr="s";
        }

        bool isdn_reverse_charge = false;        
        std::string isdn_reverse_charge_str;
        int isdn_status = Globals::k3lapi.get_param(e, "isdn_reverse_charge", isdn_reverse_charge_str);        

        if (isdn_status == ksSuccess && !isdn_reverse_charge_str.empty())
        {
            try
            {
                isdn_reverse_charge = Strings::toboolean(isdn_reverse_charge_str);
            }
            catch (Strings::invalid_value & err)
            {
            }
        }

        long int r2_category = -1;
        std::string r2_categ_a;
        int r2_status = Globals::k3lapi.get_param(e, "r2_categ_a", r2_categ_a);

        if (r2_status == ksSuccess && !r2_categ_a.empty())
        {
            try
            {
                r2_category = Strings::toulong(r2_categ_a);
            }
            catch (Strings::invalid_value e)
            {
            }

        }

        ScopedPvtLock lock(this);

        _call->_orig_addr = orig_addr;
        _call->_dest_addr = dest_addr;

        if(isdn_reverse_charge || r2_category == kg2CollectCall)
        {
            call()->_collect_call = true;            
        }

#if SWITCH_LESS_THAN(1,0,6)
        session(switch_core_session_request(Globals::khomp_pr_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL));
#else
        session(switch_core_session_request(Globals::khomp_pr_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL));
#endif

        if (justAlloc(true) != SWITCH_STATUS_SUCCESS)
        {
            setHangupCause(SWITCH_CAUSE_UNALLOCATED_NUMBER);
            cleanup(CLN_FAIL);
            LOG(ERROR, PVT_FMT(target(), "(KPR) r (Initilization Error on alloc!)"));
            return false;
        }

        /* begin context adjusting + processing */
        MatchExtension::ContextListType contexts;

        validContexts(contexts);

        std::string tmp_exten;
        std::string tmp_context;

        switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, call()->_dest_addr, call()->_orig_addr, false))
        {
            case MatchExtension::MATCH_NONE:
                destroy();
                owner(NULL);
                LOG(WARNING, PVT_FMT(_target, "(KPR) r (unable to find exten/context on incoming call %s/%s)")
                        % _call->_dest_addr % (contexts.size() >= 1 ? contexts[0] : "default"));
                return false;
            default:
                DBG(FUNC, PVT_FMT(_target, "(KPR) our: context '%s', exten '%s'") % tmp_context % tmp_exten);
                break;

        }

        call()->_incoming_context = tmp_context;
        _call->_dest_addr = tmp_exten;

        obtainBoth();

        startListen(false);

        if (justStart() != SWITCH_STATUS_SUCCESS)
        {
            setHangupCause(SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
            cleanup(CLN_FAIL);
            LOG(ERROR, PVT_FMT(target(), "(KPR) r (Initilization Error on start!)"));
            return false;
        }

        call()->_flags.set(Kflags::REALLY_CONNECTED);
        call()->statistics()->incrementNewCall();

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(KPR) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(KPR) r"));   
    return true;
}

bool BoardPassive::KhompPvtKPR::obtainBoth()
{
    /* estes buffers *NAO PODEM SER ESTATICOS*! */
    char cmd1[] = { 0x3f, 0x03, 0xff, 0x00, 0x05, 0xff };
    char cmd2[] = { 0x3f, 0x03, 0xff, 0x01, 0x05, 0xff };

    cmd1[2] = cmd1[5] = cmd2[2] = _target.object;
    cmd2[5] = _target.object + 30;

    try
    {
        int dsp = Globals::k3lapi.get_dsp(_target.device, K3LAPI::DSP_AUDIO);

        Globals::k3lapi.raw_command(_target.device, dsp, cmd1, sizeof(cmd1));

        Globals::k3lapi.raw_command(_target.device, dsp, cmd2, sizeof(cmd2));

    }
    catch(K3LAPI::failed_raw_command & e)
    {
        LOG(ERROR, PVT_FMT(target(), "ERROR sending mixer command!"));
        return false;
    }

    return true;
}

