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

#ifndef _KHOMP_PVT_PASSIVE_H_
#define _KHOMP_PVT_PASSIVE_H_

#include "khomp_pvt.h"
#include "applications.h"

/******************************************************************************/
/******************************** Passive Board *******************************/
/******************************************************************************/
struct BoardPassive: public Board
{
/******************************************************************************/
/******************************** Passive Channel *****************************/
struct KhompPvtPassive: public KhompPvt 
{
/******************************** Passive Call ********************************/
/*
    struct CallPassive : public Call
    {
        CallPassive() {}

        bool process(std::string name, std::string value = "")
        {
            }
            else
            {            
                return Call::process(name, value);
            }

            return true;
        }
        
        bool clear()
        {
            return Call::clear();
        }

    };
*/
/******************************************************************************/
    KhompPvtPassive(K3LAPIBase::GenericTarget & target) : KhompPvt(target) 
    {
    }

    ~KhompPvtPassive() 
    {
    }
/*    
    CallPassive * callPassive()
    {
        return (CallPassive *)call();
    }
*/
    
    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(Passive) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_CHANNEL_FREE:
            case EV_CHANNEL_FAIL:
                ret = onChannelRelease(e);
                break;
/*
            case EV_CALL_SUCCESS:
            case EV_CALL_FAIL:
            case EV_PULSE_DETECTED:
*/
            case EV_DISCONNECT:
            case EV_DTMF_DETECTED:
            case EV_AUDIO_STATUS:
                break;
            default:
                ret = true;
                break;
        }

        DBG(STRM, D("(Passive) r"));

        return ret;
    }
    
    bool validContexts(MatchExtension::ContextListType & contexts, 
                       std::string extra_context = "");

/*
    virtual bool cleanup(CleanupType type = CLN_HARD)
    {
        switch (type)
        {
        case CLN_HARD:
        case CLN_FAIL:
            break;
        case CLN_SOFT:
            break;
        }

        return KhompPvt::cleanup(type);
    }
*/

};
/******************************************************************************/
/********************************** HI Channel ********************************/
struct KhompPvtHI: public KhompPvtPassive
{
    KhompPvtHI(K3LAPIBase::GenericTarget & target) : KhompPvtPassive(target) 
    {
    }

    ~KhompPvtHI() 
    {
    }
    
    bool onSeizureStart(K3L_EVENT *e);

    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(HI) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_SEIZURE_START:
                ret = onSeizureStart(e);
                break;
            case EV_RING_DETECTED:
            case EV_POLARITY_REVERSAL:
                break;
            default:
                ret = KhompPvtPassive::eventHandler(e);
                break;
        }

        DBG(STRM, D("(HI) r"));

        return ret;
    }

};
/******************************************************************************/
/********************************* KPR Channel ********************************/
struct KhompPvtKPR: public KhompPvtPassive
{
    KhompPvtKPR(K3LAPIBase::GenericTarget & target) : KhompPvtPassive(target) 
    {
    }

    ~KhompPvtKPR() 
    {
    }
    
    bool onNewCall(K3L_EVENT *e);
/*
    bool onConnect(K3L_EVENT *e);
*/

    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(KPR) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_NEW_CALL:
                ret = onNewCall(e);
                break;
            case EV_CONNECT:
                break;
            case EV_SEIZURE_START:
            case EV_CAS_MFC_RECV:
            case EV_CAS_LINE_STT_CHANGED:
            case EV_LINK_STATUS:
                break;
            default:
                ret = KhompPvtPassive::eventHandler(e);
                break;
        }

        DBG(STRM, D("(KPR) r"));

        return ret;
    }

    bool obtainBoth();

};
/******************************************************************************/
/******************************************************************************/
    BoardPassive(int id) : Board(id) {}

    void initializeChannels(void)
    {
        LOG(MESSAGE, "(Passive) loading channels ...");

        for (unsigned obj = 0; obj < Globals::k3lapi.channel_count(_device_id); obj++)
        {
            K3LAPIBase::GenericTarget tgt(Globals::k3lapi, K3LAPIBase::GenericTarget::CHANNEL, _device_id, obj);
            KhompPvt * pvt;

            switch(Globals::k3lapi.channel_config(_device_id, obj).Signaling)
            {
            case ksigAnalog:
                pvt = new BoardPassive::KhompPvtHI(tgt);
                pvt->_call = new BoardPassive::KhompPvtHI::Call();
                DBG(FUNC, "(Passive) HI channel");
                break;
            CASE_RDSI_SIG:
            CASE_R2_SIG:
            CASE_FLASH_GRP:
            case ksigAnalogTerminal:
                pvt = new BoardPassive::KhompPvtKPR(tgt);
                pvt->_call = new BoardPassive::KhompPvtKPR::Call();
                DBG(FUNC, "(Passive) KPR channel");
                break;
            default:
                pvt = new Board::KhompPvt(tgt);
                pvt->_call = new Board::KhompPvt::Call();
                DBG(FUNC, FMT("(Passive) signaling %d unknown") % Globals::k3lapi.channel_config(_device_id, obj).Signaling);
                break;
            }

            _channels.push_back(pvt);

            pvt->cleanup();

        }
    }

    /*
    virtual bool eventHandler(const int obj, K3L_EVENT *e)
    {
        DBG(STRM, D("(Passive Board) c"));

        bool ret = true;

        switch(e->Code)
        {
        case :
            break;
        default:
            ret = Board::eventHandler(obj, e);
            break;
        }

        DBG(STRM, D("(Passive Board) r"));

        return ret;
    }
    */
   
};
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
#endif /* _KHOMP_PVT_PASSIVE_H_*/

