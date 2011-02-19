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

#include "cli.h"
#include "k3l.h"
#include "lock.h"
#include "khomp_pvt.h"
#include "spec.h"

/* our stream and interface handlers */
switch_stream_handle_t *Cli::stream;
switch_loadable_module_interface_t **Cli::module_interface;

/* contains all commands */
Cli::Commands Cli::_commands;

/* khomp all commands usage */
std::string Cli::_khomp_usage;

/* register our commands, but you must create the command function */
void Cli::registerCommands(APIFunc func,switch_loadable_module_interface_t **mod_int)
{
    if(!mod_int || !*mod_int)
    {
        LOG(ERROR,"Invalid module passed");
        return;
    }

    /* the manual */
    _khomp_usage =                                                             \
"---------------------------------------------------------------------------\n"\
"---------------------------------- HELP -----------------------------------\n"\
"---------------------------------------------------------------------------\n"\
" khomp channels disconnect {all | <board> all | <board> <channel>}\n"         \
" khomp channels unblock {all | <board> all | <board> <channel>}\n"            \
" khomp clear links [<board> [<link>]]\n"                                      \
" khomp clear statistics [<board> [<channel>]]\n"                              \
" khomp dump config\n"                                                         \
" khomp get <option>\n"                                                        \
" khomp kommuter {on|off}\n"                                                   \
" khomp kommuter count\n"                                                      \
" khomp log console <options>\n"                                               \
" khomp log disk <options>\n"                                                  \
" khomp log rotate\n"                                                          \
" khomp log status\n"                                                          \
" khomp log trace isdn <what>[,<what2>[,..]]\n"                                \
" khomp log trace k3l {on|off}\n"                                              \
" khomp log trace r2 {on|off}\n"                                               \
" khomp reset links [<board> [<link>]]\n"                                      \
" khomp revision\n"                                                            \
" khomp select sim <board> <channel> <sim_card>\n"                             \
" khomp send command <board> <channel> <command> [argument]\n"                 \
" khomp send raw command <board> <dsp> <c0> <c1> [...]\n"                      \
" khomp set <option> <value>\n"                                                \
" khomp show calls [<board> [<channel>]]\n"                                    \
" khomp show channels [{<board> [<channel>]} | \n"                             \
"                      {{concise|verbose|xml} [<board> [<channel>]]}]\n"       \
" khomp show links [[errors] [{<board>} | {{concise|verbose|xml}[<board>]}]]\n"\
" khomp show statistics [{{verbose|xml} [<board> [<channel>]]} | \n"           \
"                        {detailed <board> <channel>}]\n"                      \
" khomp sms <device> <destination> <message..>\n"                              \
" khomp summary [concise|verbose|xml]\n"                                       \
"---------------------------------------------------------------------------\n\n";

    /* we need this module_inteface, is used by SWITCH_ADD_API, there's no escape */
    module_interface = mod_int;

    /* khomp cli commands */
    SWITCH_ADD_API(Globals::api_interface, "khomp", "Khomp Menu", func, _khomp_usage.c_str());

    /* insert commands in list */
    for(Commands::iterator itr = _commands.begin();itr != _commands.end();itr++)
    {
        switch_console_set_complete(STR(FMT("add khomp %s") % (*itr)->complete_name));

        /* if we have options, let's insert them */
        if((*itr)->options.size() > 0)
        {
            std::vector<std::string>::iterator itr_option = (*itr)->options.begin();
            while(itr_option != (*itr)->options.end())
            {
                switch_console_set_complete(STR(FMT("add khomp %s %s") % (*itr)->complete_name % *itr_option));
                itr_option++;
            }
        }
    }
}

/* is responsible for parse and execute all commands */
bool Cli::parseCommands(int argc, char *argv[])
{
    /*
     * DEBUG_CLI_CMD();
     */

    /* khomp summary */
    if (ARG_CMP(0, "summary"))
        return EXEC_CLI_CMD(Cli::KhompSummary);

    /* khomp show */
    else if(ARG_CMP(0, "show"))
    {
        /* khomp show calls */
        if(ARG_CMP(1, "calls"))
            return EXEC_CLI_CMD(Cli::KhompShowCalls);

        /* khomp show channels */
        if(ARG_CMP(1, "channels"))
            return EXEC_CLI_CMD(Cli::KhompShowChannels);
        
        /* khomp show links */
        if(ARG_CMP(1, "links"))
            return EXEC_CLI_CMD(Cli::KhompShowLinks);

        /* khomp show statistics */
        if(ARG_CMP(1, "statistics"))
            return EXEC_CLI_CMD(Cli::KhompShowStatistics);
    }

    /* khomp clear */
    else if(ARG_CMP(0, "clear"))
    {
        /* khomp clear links */
        if(ARG_CMP(1, "links"))
            return EXEC_CLI_CMD(Cli::KhompClearLinks);

        /* khomp clear statistics */
        if(ARG_CMP(1, "statistics"))
            return EXEC_CLI_CMD(Cli::KhompClearStatistics);
    }
   
    /* khomp dump */
    else if(ARG_CMP(0, "dump"))
    {
        /* khomp dump config */
        if(ARG_CMP(1, "config"))
            return EXEC_CLI_CMD(Cli::KhompDumpConfig);
    }
 
    /* khomp reset */
    else if(ARG_CMP(0, "reset"))
    {
        /* khomp reset links */
        if(ARG_CMP(1, "links"))
            return EXEC_CLI_CMD(Cli::KhompResetLinks);

    }
    
    /* khomp sms */
    else if(ARG_CMP(0, "sms"))
    {
        return EXEC_CLI_CMD(Cli::KhompSMS);
    }

    /* khomp log */
    else if(ARG_CMP(0, "log"))
    {
        /* khomp log console */
        if(ARG_CMP(1, "console"))
            return EXEC_CLI_CMD(Cli::KhompLogConsole);

        /* khomp log  disk */
        if(ARG_CMP(1, "disk"))
            return EXEC_CLI_CMD(Cli::KhompLogDisk);

        /* khomp log status */
        if(ARG_CMP(1, "status"))
            return EXEC_CLI_CMD(Cli::KhompLogStatus);

        /* khomp log rotate */
        if(ARG_CMP(1, "rotate"))
            return EXEC_CLI_CMD(Cli::KhompLogRotate);

        /* khomp log trace */
        if(ARG_CMP(1, "trace"))
        {
            /* khomp log trace k3l */
            if(ARG_CMP(2, "k3l"))
                return EXEC_CLI_CMD(Cli::KhompLogTraceK3L);

            /* khomp log trace isdn */
            if(ARG_CMP(2, "isdn"))
                return EXEC_CLI_CMD(Cli::KhompLogTraceISDN);

            /* khomp log trace r2 */
            if(ARG_CMP(2, "r2"))
                return EXEC_CLI_CMD(Cli::KhompLogTraceR2);
        }
    }

    /* khomp channels */
    else if(ARG_CMP(0, "channels"))
    {
        /* khomp channels disconnect */
        if(ARG_CMP(1, "disconnect"))
            return EXEC_CLI_CMD(Cli::KhompChannelsDisconnect);

        /* khomp channels unblock */
        if(ARG_CMP(1, "unblock"))
            return EXEC_CLI_CMD(Cli::KhompChannelsUnblock);
    }

    /* khomp get */
    else if(ARG_CMP(0, "get"))
        return EXEC_CLI_CMD(Cli::KhompGet);

    /* khomp set */
    else if(ARG_CMP(0, "set"))
        return EXEC_CLI_CMD(Cli::KhompSet);

    /* khomp reivision */
    else if(ARG_CMP(0, "revision"))
        return EXEC_CLI_CMD(Cli::KhompRevision);

    /* khomp send */
    else if(ARG_CMP(0, "send"))
    {
        /* khomp send command */
        if(ARG_CMP(1, "command"))
            return EXEC_CLI_CMD(Cli::KhompSendCommand);

        /* khomp send raw */
        if(ARG_CMP(1, "raw"))
            return EXEC_CLI_CMD(Cli::KhompSendRawCommand);
    }
    
    /* khomp select */
    else if(ARG_CMP(0, "select"))
    {
        /* khomp select sim */
        if(ARG_CMP(1, "sim"))
            return EXEC_CLI_CMD(Cli::KhompSelectSim);

    }

    /* khomp kommuter */
    else if(ARG_CMP(0, "kommuter"))
    {
        if(ARG_CMP(1, "on") || ARG_CMP(1, "off"))
            return EXEC_CLI_CMD(Cli::KhompKommuterOnOff);
        
        if(ARG_CMP(1, "count"))
            return EXEC_CLI_CMD(Cli::KhompKommuterCount);
    }
    
    /* if everything fails, i'm here to support */
    printKhompUsage();
    
    return false;
}

/******************************************************************************/
/******************** Defining the static initialization **********************/

Cli::_KhompSummary            Cli::KhompSummary;
Cli::_KhompShowCalls          Cli::KhompShowCalls;
Cli::_KhompShowChannels       Cli::KhompShowChannels;
Cli::_KhompShowLinks          Cli::KhompShowLinks;
Cli::_KhompShowStatistics     Cli::KhompShowStatistics;
Cli::_KhompClearLinks         Cli::KhompClearLinks;
Cli::_KhompDumpConfig         Cli::KhompDumpConfig;
Cli::_KhompClearStatistics    Cli::KhompClearStatistics;
Cli::_KhompResetLinks         Cli::KhompResetLinks;
Cli::_KhompChannelsDisconnect Cli::KhompChannelsDisconnect;
Cli::_KhompChannelsUnblock    Cli::KhompChannelsUnblock;
Cli::_KhompSMS                Cli::KhompSMS;
Cli::_KhompLogConsole         Cli::KhompLogConsole;
Cli::_KhompLogDisk            Cli::KhompLogDisk;
Cli::_KhompLogStatus          Cli::KhompLogStatus;
Cli::_KhompLogRotate          Cli::KhompLogRotate;
Cli::_KhompLogTraceK3L        Cli::KhompLogTraceK3L;
Cli::_KhompLogTraceISDN       Cli::KhompLogTraceISDN;
Cli::_KhompLogTraceR2         Cli::KhompLogTraceR2;
Cli::_KhompGet                Cli::KhompGet;
Cli::_KhompSet                Cli::KhompSet;
Cli::_KhompRevision           Cli::KhompRevision;
Cli::_KhompSendCommand        Cli::KhompSendCommand;
Cli::_KhompSendRawCommand     Cli::KhompSendRawCommand;
Cli::_KhompSelectSim          Cli::KhompSelectSim;
Cli::_KhompKommuterOnOff      Cli::KhompKommuterOnOff;
Cli::_KhompKommuterCount      Cli::KhompKommuterCount;

/******************************************************************************/
/*************************** Defining the commands ****************************/
/*!
 \brief Print a system summary for all the boards. [khomp summary]
 */
bool Cli::_KhompSummary::execute(int argc, char *argv[])
{
    if(argc < 1 || argc > 2)
    {
        printUsage(stream);
        return false;
    }

    Cli::OutputType output_type = Cli::VERBOSE;
    if(ARG_CMP(1, "concise")) output_type = Cli::CONCISE;
    if(ARG_CMP(1, "xml")) 
    {
        output_type = Cli::XML;
        createRoot("summary");
    }
        
    class_type classe = ( !_on_cli_term ? C_MESSAGE : C_CLI );
    K3L_API_CONFIG apiCfg;

    if (output_type == Cli::VERBOSE)
    {
        K::Logger::Logg2(classe, stream, " ------------------------------------------------------------------");
        K::Logger::Logg2(classe, stream, "|---------------------- Khomp System Summary ----------------------|");
        K::Logger::Logg2(classe, stream, "|------------------------------------------------------------------|");
    }

    const bool running = (k3lGetDeviceConfig(-1, ksoAPI, &apiCfg, sizeof(apiCfg)) == ksSuccess);

    if(running)
    {
        switch(output_type)
        {
            case Cli::VERBOSE:
            {
                K::Logger::Logg2(classe, stream, FMT("| K3L API %d.%d.%d [m.VPD %d] - %-38s |")
                        % apiCfg.MajorVersion % apiCfg.MinorVersion % apiCfg.BuildVersion
                        % apiCfg.VpdVersionNeeded % apiCfg.StrVersion);
            } break;

            case Cli::CONCISE:
            {
                K::Logger::Logg2(classe, stream, FMT("%d.%d.%d;%d;%s")
                        % apiCfg.MajorVersion % apiCfg.MinorVersion % apiCfg.BuildVersion
                        % apiCfg.VpdVersionNeeded % apiCfg.StrVersion);
            } break;

            case Cli::XML:
            {
                /* summary/k3lapi */
                switch_xml_t xk3lapi = switch_xml_add_child_d(root, "k3lapi",0);
                
                /* summary/k3lapi/version */
                switch_xml_t xk3l_version = switch_xml_add_child_d(xk3lapi,"version", 0);
                switch_xml_set_attr_d(xk3l_version,"major",STR(FMT("%d") % apiCfg.MajorVersion));
                switch_xml_set_attr_d(xk3l_version,"minor",STR(FMT("%d") % apiCfg.MinorVersion));
                switch_xml_set_attr_d(xk3l_version,"build",STR(FMT("%d") % apiCfg.BuildVersion));
                switch_xml_set_attr_d(xk3l_version,"vpd",STR(FMT("%d")   % apiCfg.VpdVersionNeeded));

                /* summary/k3lapi/version/revision */
                switch_xml_t xk3l_rev = switch_xml_add_child_d(xk3l_version,"revision",0);
                switch_xml_set_txt_d(xk3l_rev,STR(FMT("%s") % apiCfg.StrVersion));
            } break;

            default:
                break;
        }
    }
    else
    {
        switch(output_type)
        {
            case Cli::VERBOSE:
            {
                K::Logger::Logg2(classe, stream, "| Connection to KServer broken, please check system logs!          |");
            } break;

            case Cli::CONCISE:
            {
                K::Logger::Logg2(classe, stream, "CONNECTION BROKEN");
            } break;

            case Cli::XML:
            {
                 /* summary/k3lapi */
                switch_xml_t xk3lapi = switch_xml_add_child_d(root, "k3lapi",0);
                switch_xml_set_txt_d(xk3lapi, "CONNECTION BROKEN");
            } break;
        }
    }

#ifndef MOD_KHOMP_VERSION
#define MOD_KHOMP_VERSION "unknown"
#endif

#ifndef SWITCH_VERSION_FULL
#define SWITCH_VERSION_FULL "unknown"
#endif

    std::string khomp_endpoint_rev(MOD_KHOMP_VERSION);
    std::string freeswitch_rev(SWITCH_VERSION_FULL);

    switch(output_type)
    {
        case Cli::VERBOSE:
        {
            K::Logger::Logg2(classe, stream, FMT("| Khomp Endpoint - %-47s |") % khomp_endpoint_rev);
            K::Logger::Logg2(classe, stream, FMT("| FreeSWITCH - %-51s |") % freeswitch_rev);
        } break;

        case Cli::CONCISE:
        {
            K::Logger::Logg2(classe, stream, FMT("%s") % khomp_endpoint_rev);
            K::Logger::Logg2(classe, stream, FMT("%s") % freeswitch_rev);
        } break;

        case Cli::XML:
        {
            /* summary/mod_khomp */
            switch_xml_t xmod_khomp = switch_xml_add_child_d(root,"mod_khomp",0);

            /* summary/mod_khomp/revision */
            switch_xml_t xrevision  = switch_xml_add_child_d(xmod_khomp,"revision",0);
            switch_xml_set_txt_d(xrevision, khomp_endpoint_rev.c_str());

            /* summary/freeswitch */
            switch_xml_t xfs = switch_xml_add_child_d(root,"freeswitch",0);

            /* summary/freeswitch/revision */
            switch_xml_t xfs_rev = switch_xml_add_child_d(xfs,"revision",0);
            switch_xml_set_txt_d(xfs_rev, freeswitch_rev.c_str());
        } break;

        default:
            break;
    }

    if(!running)
    {
        if (output_type == Cli::VERBOSE)
            K::Logger::Logg2(classe,stream, " ------------------------------------------------------------------");
      
        if (output_type == Cli::XML)
        {
            printXMLOutput(stream);
            clearRoot();
        }

        return false;
    }

    if (output_type == Cli::XML)
    {
        /* summary/board */
        xdevs = switch_xml_add_child_d(root,"devices",0);
    }

    for (unsigned int i = 0; i < Globals::k3lapi.device_count(); i++)
    {
        const K3L_DEVICE_CONFIG & devCfg = Globals::k3lapi.device_config(i);

        std::string tipo = Verbose::deviceName((KDeviceType)Globals::k3lapi.device_type(i), devCfg.DeviceModel);

        if (output_type == Cli::VERBOSE)
            K::Logger::Logg2(classe, stream, " ------------------------------------------------------------------");

        switch (Globals::k3lapi.device_type(i))
        {
            /* E1 boards */
            case kdtE1:
            case kdtConf:
            case kdtPR:
            case kdtE1GW:
            case kdtE1IP:
            case kdtE1Spx:
            case kdtGWIP:
            case kdtFXS:
            case kdtFXSSpx:
            case kdtE1FXSSpx:
            {
                K3L_E1600A_FW_CONFIG dspAcfg;
                K3L_E1600B_FW_CONFIG dspBcfg;

                if ((k3lGetDeviceConfig(i, ksoFirmware + kfiE1600A, &dspAcfg, sizeof(dspAcfg)) == ksSuccess) &&
                    (k3lGetDeviceConfig(i, ksoFirmware + kfiE1600B, &dspBcfg, sizeof(dspBcfg)) == ksSuccess))
                {
                    switch(output_type)
                    {
                        case Cli::VERBOSE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("| [[ %02u ]] %s, serial '%s', %02d channels, %d links.%s|")
                                    % i % tipo % devCfg.SerialNumber % devCfg.ChannelCount % devCfg.LinkCount
                                    % std::string(std::max<int>(0, 22 - tipo.size() - strlen(devCfg.SerialNumber)), ' '));
                            K::Logger::Logg2(classe, stream, FMT("| * DSP A: %s, DSP B: %s - PCI bus: %02d, PCI slot: %02d %s|")
                                    % dspAcfg.DspVersion % dspBcfg.DspVersion % devCfg.PciBus % devCfg.PciSlot
                                    % std::string(18 - strlen(dspAcfg.DspVersion) - strlen(dspBcfg.DspVersion), ' '));
                            K::Logger::Logg2(classe, stream, FMT("| * %-62s |") % dspAcfg.FwVersion);
                            K::Logger::Logg2(classe, stream, FMT("| * %-62s |") % dspBcfg.FwVersion);

                            K::Logger::Logg2(classe, stream, FMT("| * Echo Canceller: %-20s - Location: %-12s  |")
                                    % Verbose::echoCancellerConfig(devCfg.EchoConfig)
                                    % Verbose::echoLocation(devCfg.EchoLocation));
                        } break;

                        case Cli::CONCISE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("%02u;%s;%d;%d;%d;%s;%s;%02d;%02d;%s;%s;%s;%s")
                                    % i % tipo % atoi(devCfg.SerialNumber) % devCfg.ChannelCount % devCfg.LinkCount
                                    % dspAcfg.DspVersion % dspBcfg.DspVersion % devCfg.PciBus % devCfg.PciSlot
                                    % dspAcfg.FwVersion % dspBcfg.FwVersion
                                    % Verbose::echoCancellerConfig(devCfg.EchoConfig)
                                    % Verbose::echoLocation(devCfg.EchoLocation));
                        } break;

                        case Cli::XML:
                        {
                            /* boards/board */
                            switch_xml_t xdev = switch_xml_add_child_d(xdevs,"device",0);
                            switch_xml_set_attr_d(xdev,"id",STR(FMT("%02u") % i));

                            /* boards/board/general */
                            switch_xml_t xgeneral = switch_xml_add_child_d(xdev,"general",0);
    
                            /* boards/board/general/type */
                            switch_xml_t xtype = switch_xml_add_child_d(xgeneral,"type",0);
                            switch_xml_set_txt_d(xtype,tipo.c_str());

                            /* boards/board/general/serial */
                            switch_xml_t xserial = switch_xml_add_child_d(xgeneral,"serial",0);
                            switch_xml_set_txt_d(xserial, devCfg.SerialNumber);

                            /* boards/board/general/channels */
                            switch_xml_t xchannels = switch_xml_add_child_d(xgeneral,"channels",0);
                            switch_xml_set_txt_d(xchannels, STR(FMT("%02d") % devCfg.ChannelCount));

                            /* boards/board/general/links */
                            switch_xml_t xlinks = switch_xml_add_child_d(xgeneral,"links",0);
                            switch_xml_set_txt_d(xlinks, STR(FMT("%d") % devCfg.LinkCount));

                            /* boards/board/hardware */
                            switch_xml_t xhardware = switch_xml_add_child_d(xdev,"hardware",0);

                            /* boards/board/hardware/dsps */
                            switch_xml_t xdsps = switch_xml_add_child_d(xhardware,"dsps",0);
                           
                            /* boards/board/hardware/dsps/dsp (0) */
                            switch_xml_t xdsp0 = switch_xml_add_child_d(xdsps,"dsp",0);
                            switch_xml_set_attr_d(xdsp0,"id","0");

                            /* boards/board/hardware/dsps/dsp/version */
                            switch_xml_t xversion0 = switch_xml_add_child_d(xdsp0,"version",0);
                            switch_xml_set_txt_d(xversion0, dspAcfg.DspVersion);

                            /* boards/board/hardware/dsps/dsp/firmware */
                            switch_xml_t xfirmware0 = switch_xml_add_child_d(xdsp0,"firmware",0);
                            switch_xml_set_txt_d(xfirmware0,dspAcfg.FwVersion);

                            /* boards/board/hardware/dsps/dsp (1) */
                            switch_xml_t xdsp1 = switch_xml_add_child_d(xdsps,"dsp",0);
                            switch_xml_set_attr_d(xdsp1,"id","1");

                            /* boards/board/hardware/dsps/dsp/version */
                            switch_xml_t xversion1 = switch_xml_add_child_d(xdsp1,"version",0);
                            switch_xml_set_txt_d(xversion1, dspBcfg.DspVersion);

                            /* boards/board/hardware/dsps/dsp/firmware */
                            switch_xml_t xfirmware1 = switch_xml_add_child_d(xdsp1,"firmware",0);
                            switch_xml_set_txt_d(xfirmware1,dspBcfg.FwVersion);
                        } break;

                        default:
                            break;
                    }
                }

                break;
            }

            /* analog boards */
            case kdtFXO:
            case kdtFXOVoIP:
            /*
            TODO: This not found
            case kdtFX:
            case kdtFXVoIP:
            */
            {
                K3L_FXO80_FW_CONFIG dspCfg;

                if (k3lGetDeviceConfig(i, ksoFirmware + kfiFXO80, &dspCfg, sizeof(dspCfg)) == ksSuccess)
                {
                    switch(output_type)
                    {
                        case Cli::VERBOSE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("| [[ %02u ]] %s, serial '%s', %02d channels. %s|")
                                        % i % tipo % devCfg.SerialNumber % devCfg.ChannelCount
                                        % std::string(std::max<int>(0, 30 - tipo.size() - strlen(devCfg.SerialNumber)), ' '));
                            K::Logger::Logg2(classe, stream, FMT("| * DSP: %s - PCI bus: %02d, PCI slot: %02d%s|")
                                        % dspCfg.DspVersion % devCfg.PciBus % devCfg.PciSlot
                                        % std::string(30 - strlen(dspCfg.DspVersion), ' '));
                            K::Logger::Logg2(classe, stream, FMT("| * %-63s|") % dspCfg.FwVersion);
                        } break;

                        case Cli::CONCISE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("%02u;%s;%d;%d;%s;%02d;%02d;%s")
                                        % i % tipo % atoi(devCfg.SerialNumber) % devCfg.ChannelCount
                                        % dspCfg.DspVersion % devCfg.PciBus % devCfg.PciSlot
                                        % dspCfg.FwVersion);
                        } break;

                        case Cli::XML:
                        {
                            /* boards/board  */
                            switch_xml_t xdev = switch_xml_add_child_d(xdevs,"device",0);
                            switch_xml_set_attr_d(xdev,"id",STR(FMT("%02u") % i));

                            /* boards/board/general  */
                            switch_xml_t xgeneral = switch_xml_add_child_d(xdev,"general",0);
    
                            /* boards/board/general/type  */
                            switch_xml_t xtype = switch_xml_add_child_d(xgeneral,"type",0);
                            switch_xml_set_txt_d(xtype,tipo.c_str());

                            /* boards/board/general/serial */
                            switch_xml_t xserial = switch_xml_add_child_d(xgeneral,"serial",0);
                            switch_xml_set_txt_d(xserial, devCfg.SerialNumber);

                            /* boards/board/general/channels */
                            switch_xml_t xchannels = switch_xml_add_child_d(xgeneral,"channels",0);
                            switch_xml_set_txt_d(xchannels, STR(FMT("%02d") % devCfg.ChannelCount));

                            /* boards/board/general/links */
                            switch_xml_t xlinks = switch_xml_add_child_d(xgeneral,"links",0);
                            switch_xml_set_txt_d(xlinks, STR(FMT("%d") % devCfg.LinkCount));

                            /* boards/hardware */
                            switch_xml_t xhardware = switch_xml_add_child_d(xdev,"hardware",0);
                            
                            /* boards/board/hardware/dsps */
                            switch_xml_t xdsps = switch_xml_add_child_d(xhardware,"dsps",0);

                            /* boards/board/hardware/dsps/dsp */
                            switch_xml_t xdsp = switch_xml_add_child_d(xdsps,"dsp",0);
                            switch_xml_set_attr_d(xdsp,"id","0");

                            /* boards/board/hardware/dsps/dsps/version */
                            switch_xml_t xversion_a = switch_xml_add_child_d(xdsp,"version",0);
                            switch_xml_set_txt_d(xversion_a, dspCfg.DspVersion);

                            /* boards/board/hardware/dsps/dsps/firmare */
                            switch_xml_t xfirmware = switch_xml_add_child_d(xdsp,"firmware",0);
                            switch_xml_set_txt_d(xfirmware,dspCfg.FwVersion);

                            /* boards/board/hardware/pci */
                            switch_xml_t xpci = switch_xml_add_child_d(xhardware,"pci",0);
                            switch_xml_set_attr_d(xpci,"bus" ,STR(FMT("%02d") % devCfg.PciBus ));
                            switch_xml_set_attr_d(xpci,"slot",STR(FMT("%02d") % devCfg.PciSlot));
                        } break;

                        default:
                            break;
                    }
                }

                if (Globals::k3lapi.device_type(i) == kdtFXOVoIP)
                {
                    if (output_type == Cli::VERBOSE)
                    {
                        K::Logger::Logg2(classe, stream, FMT("| * Echo Canceller: %-20s - Location: %-12s  |")
                            % Verbose::echoCancellerConfig(devCfg.EchoConfig)
                            % Verbose::echoLocation(devCfg.EchoLocation));
                    }
                }

                break;
            }
            case kdtGSM:
            case kdtGSMSpx:
            {
                K3L_GSM40_FW_CONFIG dspCfg;

                if (k3lGetDeviceConfig(i, ksoFirmware + kfiGSM40, &dspCfg, sizeof(dspCfg)) == ksSuccess)
                {
                    switch(output_type)
                    {
                        case Cli::VERBOSE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("| [[ %02d ]] %s, serial '%s', %02d channels. %s|")
                                % i % tipo % devCfg.SerialNumber % devCfg.ChannelCount
                                % std::string(std::max<int>(0, 30 - tipo.size() - strlen(devCfg.SerialNumber)), ' '));

                            K::Logger::Logg2(classe, stream, FMT("| * DSP: %s - PCI bus: %02d, PCI slot: %02d%s|")
                                        % dspCfg.DspVersion % devCfg.PciBus % devCfg.PciSlot
                                        % std::string(std::max<int>(30 - strlen(dspCfg.DspVersion), 0), ' '));

                            K::Logger::Logg2(classe, stream, FMT("| * %-62s |") % dspCfg.FwVersion);
                        } break;

                        case Cli::CONCISE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("%02d;%s;%d;%d;%s;%02d;%02d;%s")
                                        % i % tipo % atoi(devCfg.SerialNumber) % devCfg.ChannelCount
                                        % dspCfg.DspVersion % devCfg.PciBus % devCfg.PciSlot
                                        % dspCfg.FwVersion);
                        } break;

                        case Cli::XML:
                        {
                            /* boards/board */
                            switch_xml_t xdev = switch_xml_add_child_d(xdev,"device",0);
                            switch_xml_set_attr_d(xdev,"id",STR(FMT("%02u") % i));

                            /* boards/board/general */
                            switch_xml_t xgeneral = switch_xml_add_child_d(xdev,"general",0);
    
                            /* boards/board/general/type */
                            switch_xml_t xtype = switch_xml_add_child_d(xgeneral,"type",0);
                            switch_xml_set_txt_d(xtype,tipo.c_str());

                            /* boards/board/general/serial */
                            switch_xml_t xserial = switch_xml_add_child_d(xgeneral,"serial",0);
                            switch_xml_set_txt_d(xserial, devCfg.SerialNumber);

                            /* boards/board/general/channels */
                            switch_xml_t xchannels = switch_xml_add_child_d(xgeneral,"channels",0);
                            switch_xml_set_txt_d(xchannels, STR(FMT("%02d") % devCfg.ChannelCount));

                            /* boards/board/general/links */
                            switch_xml_t xlinks = switch_xml_add_child_d(xgeneral,"links",0);
                            switch_xml_set_txt_d(xlinks, STR(FMT("%d") % devCfg.LinkCount));

                            /* boards/board/hardware */
                            switch_xml_t xhardware = switch_xml_add_child_d(xdev,"hardware",0);

                            /* boards/board/hardware/dsps */
                            switch_xml_t xdsps = switch_xml_add_child_d(xhardware,"dsps",0);

                            /* boards/board/hardware/dsps/dsp */
                            switch_xml_t xdsp = switch_xml_add_child_d(xdsps,"dsp",0);
                            switch_xml_set_attr_d(xdsp,"id","0");

                            /* boards/board/hardware/dsps/dsp/version */
                            switch_xml_t xversion_a = switch_xml_add_child_d(xdsp,"version",0);
                            switch_xml_set_txt_d(xversion_a, dspCfg.DspVersion);

                            /* boards/board/hardware/dsps/dsp/firmware*/
                            switch_xml_t xfirmware = switch_xml_add_child_d(xdsp,"firmware",0);
                            switch_xml_set_txt_d(xfirmware,dspCfg.FwVersion);

                            /* boards/board/hardware/dsps/dsp/pci */
                            switch_xml_t xpci = switch_xml_add_child_d(xhardware,"pci",0);
                            switch_xml_set_attr_d(xpci,"bus" ,STR(FMT("%02d") % devCfg.PciBus ));
                            switch_xml_set_attr_d(xpci,"slot",STR(FMT("%02d") % devCfg.PciSlot));
                        } break;

                        default:
                            break;
                    }
                }

                break;
            }
            case kdtGSMUSB:
            case kdtGSMUSBSpx:
            {
                K3L_GSMUSB_FW_CONFIG dspCfg;

                if (k3lGetDeviceConfig(i, ksoFirmware + kfiGSMUSB, &dspCfg, sizeof(dspCfg)) == ksSuccess)
                {
                    switch(output_type)
                    {
                        case Cli::VERBOSE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("| [[ %02d ]] %s, serial '%s', %02d channels. %s|")
                                % i % tipo % devCfg.SerialNumber % devCfg.ChannelCount
                                % std::string(std::max<int>(0, 30 - tipo.size() - strlen(devCfg.SerialNumber)), ' '));

                            int size = strlen(dspCfg.DspVersion) + strlen(dspCfg.FwVersion);

                            K::Logger::Logg2(classe, stream, FMT("| * DSP: %s - %s%s|")
                                % dspCfg.DspVersion % dspCfg.FwVersion
                                % std::string(std::max<int>(55 - size, 0), ' '));
                            
                        } break;

                        case Cli::CONCISE:
                        {
                            K::Logger::Logg2(classe, stream, FMT("%02d;%s;%d;%d;%s;%s")
                                        % i % tipo % atoi(devCfg.SerialNumber) % devCfg.ChannelCount
                                        % dspCfg.DspVersion % dspCfg.FwVersion);
                        } break;

                        case Cli::XML:
                        {
                            /* boards/board */
                            switch_xml_t xdev = switch_xml_add_child_d(xdev,"device",0);
                            switch_xml_set_attr_d(xdev,"id",STR(FMT("%02u") % i));

                            /* boards/board/general */
                            switch_xml_t xgeneral = switch_xml_add_child_d(xdev,"general",0);
    
                            /* boards/board/general/type */
                            switch_xml_t xtype = switch_xml_add_child_d(xgeneral,"type",0);
                            switch_xml_set_txt_d(xtype,tipo.c_str());

                            /* boards/board/general/serial */
                            switch_xml_t xserial = switch_xml_add_child_d(xgeneral,"serial",0);
                            switch_xml_set_txt_d(xserial, devCfg.SerialNumber);

                            /* boards/board/general/channels */
                            switch_xml_t xchannels = switch_xml_add_child_d(xgeneral,"channels",0);
                            switch_xml_set_txt_d(xchannels, STR(FMT("%02d") % devCfg.ChannelCount));

                            /* boards/board/hardware */
                            switch_xml_t xhardware = switch_xml_add_child_d(xdev,"hardware",0);
                            
                            /* boards/board/hardware/dsps */
                            switch_xml_t xdsps = switch_xml_add_child_d(xhardware,"dsps",0);

                            /* boards/board/hardware/dsp */
                            switch_xml_t xdsp = switch_xml_add_child_d(xdsps,"dsp",0);
                            switch_xml_set_attr_d(xdsp,"id","0");

                            /* boards/board/hardware/dsps/dsp/version */
                            switch_xml_t xversion_a = switch_xml_add_child_d(xdsp,"version",0);
                            switch_xml_set_txt_d(xversion_a, dspCfg.DspVersion);

                            /* boards/board/hardware/dsps/dsp/firmware */
                            switch_xml_t xfirmware = switch_xml_add_child_d(xdsp,"firmware",0);
                            switch_xml_set_txt_d(xfirmware,dspCfg.FwVersion);
                        } break;

                        default:
                            break;
                    }
                }

                break;
            }

            default:
                K::Logger::Logg2(classe, stream, FMT("| [[ %02d ]] Unknown type '%02d'! Please contact Khomp support for help! |")
                    % i % Globals::k3lapi.device_type(i));
                break;
        }
    }

    if (output_type == Cli::VERBOSE)
        K::Logger::Logg2(classe,stream, " ------------------------------------------------------------------");
      
    if (output_type == Cli::XML)
    {
        printXMLOutput(stream);
        clearRoot();
    }

    return true;
}

/* support function for _KhompShowCalls */
void Cli::_KhompShowCalls::showCalls(unsigned int d, unsigned int o, std::string &buffer)
{
    buffer += STR(FMT("| %d,%02d |   unused   | %11s | %-36s |\n")
             % d
             % o
             % Globals::k3lutil.callStatus(d,o)
             % Globals::k3lutil.channelStatus(d,o));
}

bool Cli::_KhompShowCalls::execute(int argc, char *argv[])
{
    if(argc > 4)
    {
        printUsage(stream);
        return false;
    }

    int device = argv[2] ? atoi(argv[2]) : -1; 
    int object = argv[3] ? atoi(argv[3]) : -1; 

    int d = -1;
    int o = -1;

    if (device != -1)
    {    
        d = device;

        if (!Globals::k3lapi.valid_device(d))
        {    
            K::Logger::Logg2(C_CLI, stream, FMT("ERROR: No such device %d!") % d);
            return false;
        }    
    }   

    if (object != -1)
    {    
        o = object;

        if (!Globals::k3lapi.valid_channel(d,o))
        {    
            K::Logger::Logg2(C_CLI, stream, FMT("ERROR: No such chanel %d for device %d!") % o % d);
            return false;
        }    
    } 

    /* keep the channels reponse */
    std::string buffer("");

    try
    {
        /**/ if (d != -1 && o != -1)
        {    
            showCalls(d,o,buffer);
        }    
        else if (d != -1 && o == -1)
        {    
            for (unsigned int i = 0; i < Globals::k3lapi.channel_count(d); i++) 
            {    
                buffer += " ------------------------------------------------------------------------\n";
                showCalls(d,i,buffer);
            }    
        }    
        else if (d == -1 && o == -1)
        {    
            for (unsigned int i = 0; i < Globals::k3lapi.device_count(); i++) 
            {    
                buffer += " ------------------------------------------------------------------------\n";
                for (unsigned int j = 0; j < Globals::k3lapi.channel_count(i); j++)
                {
                    showCalls(i,j,buffer);
                }
            }    
        }
    }
    catch(K3LAPITraits::invalid_channel & err)
    {
        K::Logger::Logg2(C_CLI, stream, "ERROR: No such chanel");
        return false;
    }

    //TODO: The information shown here, must be reviewed cuz is the same of show channels
    K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------");
    K::Logger::Logg2(C_CLI, stream, "|------------------------------- Khomp Calls ----------------------------|");
    K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------ ");
    K::Logger::Logg2(C_CLI, stream, "|  hw  | freeSWITCH |  khomp call |             khomp channel            |");
    K::Logger::Logg2(C_CLI, stream, "|  id  |   status   |    status   |                status                |");
    K::Logger::Logg2(C_CLI, stream, "%s", (char*) buffer.c_str());
    K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------");

    return true;
}

bool Cli::_KhompChannelsDisconnect::forceDisconnect(unsigned int device, unsigned int channel)
{
    bool ret = false;
    Board::KhompPvt *pvt = NULL;

    try
    {
        pvt = Board::get(device,channel);
        ScopedPvtLock lock(pvt);

        ret = pvt->command(KHOMP_LOG,CM_DISCONNECT);
        DBG(FUNC,PVT_FMT(pvt->target(),"Command CM_DISCONNECT sent!"));
    }
    catch (K3LAPITraits::invalid_channel & err)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
    }
    catch (ScopedLockFailed & err)
    {
        K::Logger::Logg2(C_CLI,stream,FMT("error: channel %d at device %d could not be locked: %s!")
        % channel 
        % device
        % err._msg.c_str());
    }

    return ret;
}

bool Cli::_KhompChannelsDisconnect::execute(int argc, char *argv[])
{
    if (argc < 3 || argc > 4)
    {
        printUsage(stream);
        return false;
    }

    switch (argc)
    {    
        case 3:
        {    
            if (!ARG_CMP(2, "all"))
            {    
                K::Logger::Logg2(C_CLI, stream, "usage: khomp channels disconnect < all | <boardid> < all | <channelid> > >");
                return false;
            }    

            K::Logger::Logg2(C_CLI, stream, "NOTICE: Disconnecting all channels on all boards!");

            for (unsigned int dev = 0; dev < Globals::k3lapi.device_count(); dev++)
            {    
                for (unsigned int chan = 0; chan < Globals::k3lapi.channel_count(dev); chan++)
                    forceDisconnect(dev,chan);
            }    
            break;
        }    
        case 4:
        {    
            unsigned int dev = atoi(argv[2]);

            if (ARG_CMP(3, "all"))
            {    
                K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Disconnecting all channels on board %d!") % dev);

                for (unsigned int chan = 0; chan < Globals::k3lapi.channel_count(dev); chan++)
                    forceDisconnect(dev,chan);
            }    

            unsigned int channel = atoi(argv[3]);

            if (dev >= Globals::k3lapi.device_count())
            {    
                K::Logger::Logg2(C_CLI, stream, FMT("ERROR: No such device: %d!") % dev);
                return false;
            }    

            
            K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Disconnecting channel %d on board %d!") % channel % dev);

            forceDisconnect(dev,channel);
            break;
        }    

        default:
            break;
    }   

    return true;
}

bool Cli::_KhompChannelsUnblock::execute(int argc, char *argv[])
{
    if (argc != 4 && argc != 3)
    {    
        printUsage(stream);
        return false;
    }    

    switch (argc)
    {    
        case 3:
        {    
            K::Logger::Logg2(C_CLI, stream, "NOTICE: Unblocking all channels on all devices!");
            for (unsigned int dev = 0; dev < Globals::k3lapi.device_count(); dev++)
            {    
                for (unsigned int chan = 0; chan < Globals::k3lapi.channel_count(dev); chan++)
                {    
                    try
                    {
                        Globals::k3lapi.command(dev,chan,CM_UNLOCK_INCOMING);
                        Globals::k3lapi.command(dev,chan,CM_UNLOCK_OUTGOING);
                    }
                    catch(K3LAPI::failed_command &e)
                    {
                        if (K::Logger::Logg.classe(C_WARNING).enabled())
                        {   
                            LOG(WARNING, FMT("Command '%s' has failed with error '%s'.")
                                    % Verbose::commandName(e.code) % Verbose::status((KLibraryStatus)e.rc));
                        }   

                        return false;
                    }
                }    
            }    
            break;
        }    
        case 4:
        {    
            int dev = atoi (argv[2]);

            if ( !SAFE_strcasecmp(argv[3], "all") )
            {    
                if (!Globals::k3lapi.valid_device( dev ))
                {    
                    K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find device: %d!") % dev );
                    return false;
                }    

                K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Unblocking all channels on device %d!") % dev);

                for (unsigned int i = 0; i < Globals::k3lapi.channel_count(dev); i++) 
                {
                    try
                    {
                        Globals::k3lapi.command(dev,i,CM_UNLOCK_INCOMING);
                        Globals::k3lapi.command(dev,i,CM_UNLOCK_OUTGOING);
                    }
                    catch(K3LAPI::failed_command &e)
                    {
                        if (K::Logger::Logg.classe(C_WARNING).enabled())
                        {   
                            LOG(WARNING, FMT("Command '%s' has failed with error '%s'.")
                                    % Verbose::commandName(e.code) % Verbose::status((KLibraryStatus)e.rc));
                        }   

                        return false;
                    }
                }
            } 
            else 
            {    
                int obj = atoi (argv[3]);

                if (!Globals::k3lapi.valid_channel(dev, obj))
                {    
                    K::Logger::Logg2(C_CLI, stream, FMT("ERROR: No such channel %d at device %d!") % obj % dev);
                    return false;
                }    

                K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Unblocking channel %d on device %d!") % obj % dev);

                try
                {
                    Globals::k3lapi.command(dev,obj,CM_UNLOCK_INCOMING);
                    Globals::k3lapi.command(dev,obj,CM_UNLOCK_OUTGOING);
                }
                catch(K3LAPI::failed_command &e)
                {
                    if (K::Logger::Logg.classe(C_WARNING).enabled())
                    {   
                        LOG(WARNING, FMT("Command '%s' has failed with error '%s'.")
                                % Verbose::commandName(e.code) % Verbose::status((KLibraryStatus)e.rc));
                    }   

                    return false;
                }
            }    
            break;
        }    
        default:
            break;
    }    
    
    return true;
}

void Cli::_KhompShowStatistics::cliStatistics(unsigned int device, OutputType output_type)
{
    if(output_type == Cli::XML)
    {
        /* device */
        xdevs = switch_xml_add_child_d(root,"device",0);
        switch_xml_set_attr_d(xdevs,"id",STR(FMT("%d") % device));
    }

    for (unsigned int channel = 0; channel < Globals::k3lapi.channel_count(device); channel++)
    {
        try
        {
            Board::KhompPvt *pvt = Board::get(device, channel);
            switch(output_type)
            {
                case Cli::VERBOSE:
                    K::Logger::Logg2(C_CLI,stream,pvt->getStatistics(Statistics::ROW).c_str());
                    break;
                case Cli::CONCISE:
                    /* do we need concise ? */
                    break;
                case Cli::XML:
                    switch_xml_insert(pvt->getStatisticsXML(Statistics::ROW),xdevs,0);
                    break;
            }
        }
        catch (K3LAPITraits::invalid_channel & err)
        {
            K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
        }
    }    
}

void Cli::_KhompShowStatistics::cliDetailedStatistics(unsigned int device, unsigned int channel, OutputType output_type)
{
    try
    {
        Board::KhompPvt *pvt = Board::get(device, channel);
        switch(output_type)
        {
            case Cli::DETAILED:
            {
                K::Logger::Logg2(C_CLI,stream,"----------------------------------------------");
                K::Logger::Logg2(C_CLI,stream,FMT("Detailed statistics of: Device %02d - Channel %02d") % pvt->target().device % pvt->target().object);
                K::Logger::Logg2(C_CLI,stream,pvt->getStatistics(Statistics::DETAILED).c_str());
                K::Logger::Logg2(C_CLI,stream,"----------------------------------------------");
            } break;

            case Cli::CONCISE:
            {
                /* We don't have xml, need concise yet ? */
            } break;

            case Cli::XML:
            {
                /* We don't have concise, need XML yet ? */
            } break;
        }
    }
    catch (K3LAPITraits::invalid_channel & err)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
    }
}

bool Cli::_KhompShowStatistics::execute(int argc, char *argv[])
{
    if (argc < 2 || argc > 5) 
    {
        printUsage(stream);
        return false;
    }

    unsigned int dev = UINT_MAX;
    unsigned int obj = UINT_MAX;
    int detailed = 0, verbose  = 0, as_xml = 0; 
    OutputType output_type = Cli::VERBOSE;

    detailed = ((argc > 2)  && (!strcasecmp(argv[2], "detailed")) ? 1 : 0 ); 
    verbose  = ((argc > 2)  && (!strcasecmp(argv[2], "verbose"))  ? 1 : 0 ); 
    as_xml   = ((argc > 2)  && (!strcasecmp(argv[2], "xml"))      ? 1 : 0 ); 

    try  
    {   
        if(argc > (2+detailed+verbose+as_xml))
        {
            dev = Strings::tolong(argv[2+detailed+verbose+as_xml]);
            if (!Globals::k3lapi.valid_device(dev))
            {    
                K::Logger::Logg2(C_CLI, stream, "ERROR: No such device!");
                return false;
            }    
        }

        if (argc > (3+detailed+verbose+as_xml))
        {   
            std::string object(argv[3+detailed+verbose+as_xml]);
            obj = Strings::tolong(object);

            if (!Globals::k3lapi.valid_channel(dev, obj))
            {    
                K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % obj % dev );
                return false;
            }  
        }
    }    
    catch (Strings::invalid_value e)
    {    
        K::Logger::Logg2(C_CLI, stream, "ERROR: Invalid numeric value!");
        return false;
    }

    if(detailed) output_type = Cli::DETAILED;
    if(as_xml) 
    {
        createRoot("statistics");
        output_type = Cli::XML;
    }
    
    std::string header;
    header.append( " ------------------------------------------------------------------------------------\n");
    header.append( "|----------------------------- Khomp Endpoint Statistics ----------------------------|\n");
    header.append( "|------------------------------------------------------------------------------------|\n");
    header.append( "|  hw  |          total calls           | channel | FreeSWITCH | channel  |  status  |\n");
    header.append( "|  id  | incoming | outgoing |  failed  |  fails  |   status   |  state   |   time   |\n");
    header.append( " ------------------------------------------------------------------------------------");
    std::string footer;
    footer.append( " ------------------------------------------------------------------------------------");

    try
    {
        if (obj != UINT_MAX)
        {
            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI,stream,header.c_str());
                    Board::KhompPvt *pvt = Board::get(dev, obj);
                    K::Logger::Logg2(C_CLI,stream,pvt->getStatistics(Statistics::ROW).c_str());
                    K::Logger::Logg2(C_CLI,stream,footer.c_str());
                } break;

                case Cli::DETAILED:
                {
                    cliDetailedStatistics (dev, obj, output_type);
                } break;

                case Cli::XML:
                {
                    /* no problem, nothing created */
                    switch_xml_t xboard = NULL;

                    /* device */
                    xdevs = switch_xml_add_child_d(root,"device",0);
                    switch_xml_set_attr_d(xdevs,"id",STR(FMT("%d") % dev));

                    try
                    {
                        Board::KhompPvt *pvt = Board::get(dev, obj);
                        switch_xml_insert(pvt->getStatisticsXML(Statistics::ROW),xdevs,0);
                    }
                    catch (K3LAPITraits::invalid_channel & err)
                    {
                        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
                    }

                    printXMLOutput(stream);
                    clearRoot();
                } break;
            }
        }
        else
        {
            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI,stream,header.c_str());

                    if (dev == UINT_MAX)
                    {
                        for (dev = 0; dev < Globals::k3lapi.device_count(); dev++)
                            cliStatistics (dev, output_type);
                    }
                    else
                    {
                        cliStatistics (dev, output_type);
                    }

                    K::Logger::Logg2(C_CLI,stream,footer.c_str());
                } break;

                case Cli::XML:
                {
                    if (dev == UINT_MAX)
                    {
                        for (dev = 0; dev < Globals::k3lapi.device_count(); dev++)
                            cliStatistics (dev, output_type);
                    }
                    else
                    {
                        cliStatistics (dev, output_type);
                    }

                    printXMLOutput(stream);
                    clearRoot();

                } break;

                case Cli::DETAILED:
                {
                    printUsage(stream);
                    return false;
                } break;
            }
        }
    }
    catch(K3LAPITraits::invalid_channel &e)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % obj % dev );
    }

    return true;
}

void Cli::_KhompShowChannels::showChannel(unsigned int device, unsigned int channel, OutputType output_type)
{
    try
    {
        Board::KhompPvt *pvt = Board::get(device, channel);

        DBG(FUNC, PVT_FMT(pvt->target(), "found channel.."));

        /* skip inactive channels */
        if (pvt->getSignaling() == ksigInactive)
            return;

        DBG(FUNC, PVT_FMT(pvt->target(), "is valid.."));

        /* make sure the states wont start 'dancing' at random.. */
        ScopedPvtLock lock(pvt);

        std::string tmp_call = Globals::k3lutil.callStatus(
                pvt->target().device, 
                pvt->target().object, 
                (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));

        std::string tmp_chan = Globals::k3lutil.channelStatus(
                pvt->target().device, 
                pvt->target().object, 
                (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));


        switch(output_type)
        {
            case Cli::VERBOSE:
            {
                if (pvt->getSignaling() == ksigGSM)
                {    
                    K3L_GSM_CHANNEL_STATUS gsmStatus;

                    if (k3lGetDeviceStatus(device, channel + ksoGsmChannel, &gsmStatus, sizeof(gsmStatus)) != ksSuccess)
                        return;

                    const unsigned int sign_numb = (gsmStatus.SignalStrength != 255 ? gsmStatus.SignalStrength : 0);

                    const unsigned int full_size = 10;
                    const unsigned int sign_size = std::min((sign_numb * full_size) / 100, full_size);

                    std::string tmp_antenna_level;

                    for (unsigned int i = 0; i < sign_size; i++) 
                        tmp_antenna_level += '*'; 

                    for (unsigned int i = sign_size; i < full_size; i++) 
                        tmp_antenna_level += ' '; 

                    tmp_chan += " (";
                    tmp_chan += (strlen(gsmStatus.OperName) != 0 ? gsmStatus.OperName : "...");
                    tmp_chan += ")"; 

                    K::Logger::Logg2(C_CLI, stream, FMT("| %d,%02d | %8s | %8s | %-23s | %02d%% |%s|")
                            % device % channel %  pvt->getStateString() % tmp_call % tmp_chan
                            % sign_numb % tmp_antenna_level);
                }    
                else 
                {    
                    K::Logger::Logg2(C_CLI, stream, FMT("| %d,%02d | %8s | %8s | %-40s |")
                            % device % channel % pvt->getStateString() % tmp_call % tmp_chan);
                }    

            } break;

            case Cli::CONCISE:   
            {
                std::string state = pvt->getStateString();

                if (pvt->getSignaling() == ksigGSM)
                {    
                    K3L_GSM_CHANNEL_STATUS gsmStatus;

                    if (k3lGetDeviceStatus(device, channel + ksoGsmChannel, &gsmStatus, sizeof(gsmStatus)) != ksSuccess)
                        return;

                    const unsigned int sign_numb = (gsmStatus.SignalStrength != 255 ? gsmStatus.SignalStrength : 0);

                    std::string gsm_registry = (strlen(gsmStatus.OperName) != 0 ? gsmStatus.OperName : "<none>");

                    K::Logger::Logg2(C_CLI, stream, FMT("B%02dC%02d:%s:%s:%s:%d%%:%s")
                            % device % channel % state % tmp_call % tmp_chan
                            % sign_numb % gsm_registry);
                }    
                else 
                {    
                    K::Logger::Logg2(C_CLI, stream, FMT("B%02dC%02d:%s:%s:%s")
                            % device % channel %  state % tmp_call % tmp_chan);
                }    

            } break;

            case Cli::XML:
            {
                /* device/channel */
                switch_xml_t xchannel = switch_xml_add_child_d(xdev,"channel",0);
                switch_xml_set_attr_d(xchannel,"id",STR(FMT("%d") % channel));

                /* device/channel/fs_state */
                switch_xml_t xstate = switch_xml_add_child_d(xchannel,"fs_state",0);
                switch_xml_set_txt_d(xstate, pvt->getStateString().c_str());
               
                /* device/channel/call */
                switch_xml_t xcall = switch_xml_add_child_d(xchannel,"call",0);
                switch_xml_set_txt_d(xcall, tmp_call.c_str());

                /* device/channel/status */
                switch_xml_t xstatus = switch_xml_add_child_d(xchannel,"status",0);
                switch_xml_set_txt_d(xstatus, tmp_chan.c_str());

                if (pvt->getSignaling() == ksigGSM)
                {   
                    K3L_GSM_CHANNEL_STATUS gsmStatus;

                    if (k3lGetDeviceStatus(device, channel + ksoGsmChannel, &gsmStatus, sizeof(gsmStatus)) != ksSuccess)
                        return;

                    const unsigned int sign_numb = (gsmStatus.SignalStrength != 255 ? gsmStatus.SignalStrength : 0);
                    std::string gsm_registry = (strlen(gsmStatus.OperName) != 0 ? gsmStatus.OperName : "<none>");

                    /* device/channel/signal */
                    switch_xml_t xsign_numb = switch_xml_add_child_d(xchannel, "signal", 0);
                    switch_xml_set_txt_d(xsign_numb, STR(FMT("%d") % sign_numb));

                    /* device/channel/registry */
                    switch_xml_t xgsm_registry = switch_xml_add_child_d(xchannel, "registry", 9);
                    switch_xml_set_txt_d(xgsm_registry, gsm_registry.c_str());
                }    
            
            } break;

            default:
                /* do nothing */ 
                break;
        }

    }    
    catch (K3LAPITraits::invalid_channel & err)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
    }
    catch (...)
    {    
        K::Logger::Logg2(C_CLI, stream, "ERROR: Unexpected error..., skipping");
    }   
}

void Cli::_KhompShowChannels::showChannels(unsigned int device, OutputType output_type)
{
    if(output_type == Cli::XML)
    {
        /* channels/device */
        xdev = switch_xml_add_child_d(root,"device",0);
        switch_xml_set_attr_d(xdev, "id", STR(FMT("%d") % device));
    }

    for (unsigned int channel = 0; channel < Globals::k3lapi.channel_count(device); channel++)
    {
        showChannel(device, channel, output_type);
    }
}

bool Cli::_KhompShowChannels::execute(int argc, char *argv[])
{
    unsigned int dev = UINT_MAX;
    int concise = 0, verbose = 0, as_xml = 0;
    OutputType output_type = Cli::VERBOSE;

    bool onlyShowOneChannel = false;
    unsigned int channelToShow = 0; 

    if (argc > 5)
    {
        Cli::KhompShowChannels.printUsage(stream);
        return false;
    }

    concise = ( ((argc == 3) || (argc == 4) || (argc == 5) ) && (ARG_CMP(2, "concise")) ? 1 : 0 ); 
    verbose = ( ((argc == 3) || (argc == 4) || (argc == 5) ) && (ARG_CMP(2, "verbose")) ? 1 : 0 ); 
    as_xml  = ( ((argc == 3) || (argc == 4) || (argc == 5) ) && (ARG_CMP(2, "xml"))     ? 1 : 0 ); 

    if (argc >= (3 + concise + verbose + as_xml))
    {    
        dev = atoi (argv[2 + concise + verbose + as_xml]);

        if (!Globals::k3lapi.valid_device(dev))
        {    
            K::Logger::Logg2(C_CLI, stream, "ERROR: no such device!");
            return false;
        }    
        
        if (argc == ( 4 + concise + verbose + as_xml))
        {
            onlyShowOneChannel = true;
            channelToShow = atoi (argv[3 + concise + verbose + as_xml]);
        }    
    }    

    if (concise == 0 && as_xml == 0)
    {    
        K::Logger::Logg2(C_CLI, stream, " -----------------------------------------------------------------------");
        K::Logger::Logg2(C_CLI, stream, "|-------------------- Khomp Channels and Connections -------------------|");
        K::Logger::Logg2(C_CLI, stream, "|-----------------------------------------------------------------------|");
        K::Logger::Logg2(C_CLI, stream, "|  hw  |freeSWITCH|   call   |                   channel                |");
        K::Logger::Logg2(C_CLI, stream, "|  id  |  status  |  status  |                   status                 |");
        K::Logger::Logg2(C_CLI, stream, " -----------------------------------------------------------------------");
    } 

    if (concise != 0) output_type = Cli::CONCISE;
    if (as_xml  != 0) 
    {
        output_type = Cli::XML;

        /* channels */
        createRoot("channels");
    }

    if ( onlyShowOneChannel )
    {    
        if ( channelToShow <  Globals::k3lapi.channel_count(dev) )
        {    
            if(output_type == Cli::XML)
            {
                /* channels/device */
                xdev = switch_xml_add_child_d(root,"device",0);
                switch_xml_set_attr_d(xdev, "id", STR(FMT("%d") % dev));
            }

            showChannel (dev, channelToShow, output_type);
        }    
        else 
        {    
            K::Logger::Logg2(C_CLI, stream, "ERROR: no such channel!");
        }    
    }    
    else if (dev == UINT_MAX)
    {    
        for (dev = 0; dev < Globals::k3lapi.device_count(); dev++)
        {
            showChannels(dev, output_type);
        }
    }    
    else 
    {    
        showChannels ( dev, output_type);
    }

    if (concise == 0 && as_xml == 0)
        K::Logger::Logg2(C_CLI, stream, " -----------------------------------------------------------------------");

    if(output_type == Cli::XML)
    {
        printXMLOutput(stream);
        clearRoot();
    }

    return true;
}

std::string Cli::_KhompShowLinks::getLinkStatus(int dev, int obj, Verbose::Presentation fmt)
{
    switch(Globals::k3lapi.device_type(dev))
    {
        case kdtE1FXSSpx:
            if (obj == 1)
                return Globals::k3lutil.linkStatus(dev, obj, fmt, ksigAnalogTerminal, true);
        default:
            break;
    }

    std::string res;

    try  
    {    
        const K3L_LINK_CONFIG & conf = Globals::k3lapi.link_config(dev, obj);
        
        res = Globals::k3lutil.linkStatus(dev, obj, fmt);

        if (conf.ReceivingClock & 0x01)
           res += (fmt == Verbose::EXACT ? ",sync" : " (sync)");
    }    
    catch (K3LAPITraits::invalid_target & e) 
    {    
        res = "<error>";
    }    

    return res; 
}

void Cli::_KhompShowLinks::showLinks(unsigned int device, OutputType output_type)
{
    if (output_type != Cli::CONCISE && output_type != Cli::XML)
        K::Logger::Logg2(C_CLI, stream, "|------------------------------------------------------------------------|");

    switch (Globals::k3lutil.physicalLinkCount(device, true))
    {
        case 1:
        {
            std::string str_link0 = getLinkStatus(device, 0, (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));
        
            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI, stream, FMT("| Link '0' on board '%d': %-47s |") % device % str_link0);
                } break;

                case Cli::CONCISE:
                {
                    K::Logger::Logg2(C_MESSAGE, stream, FMT("B%02dL00:%s") % device % str_link0);
                } break;

                case Cli::XML:
                {
                    /* device */
                    xdev = switch_xml_add_child_d(root,"device",0);
                    switch_xml_set_attr_d(xdev,"id",STR(FMT("%d") % device));
                  
                    /* device/links */
                    switch_xml_t xlinks = switch_xml_add_child_d(xdev,"links",0);

                    /* device/links/link */
                    switch_xml_t xlink = switch_xml_add_child_d(xlinks,"link",0);
                    switch_xml_set_attr_d(xlink, "id", "0");
                    switch_xml_set_txt_d(xlink, str_link0.c_str());
                } break;
            }

            break;
        }

        case 2:
        {
            std::string str_link0 = getLinkStatus(device, 0, (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));
            std::string str_link1 = getLinkStatus(device, 1, (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));

            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI, stream, FMT("|------ Link '0' on board '%d' ------||------ Link '1' on board '%d' ------|")
                        % device % device);

                    K::Logger::Logg2(C_CLI, stream, FMT("| %-33s || %-33s |") % str_link0 % str_link1);
                } break;

                case Cli::CONCISE:
                {
                    K::Logger::Logg2(C_MESSAGE, stream, FMT("B%02dL00:%s") % device % str_link0);
                    K::Logger::Logg2(C_MESSAGE, stream, FMT("B%02dL01:%s") % device % str_link1);
                } break;

                case Cli::XML:
                {
                    /* device */
                    xdev = switch_xml_add_child_d(root,"device",0);
                    switch_xml_set_attr_d(xdev,"id",STR(FMT("%d") % device));
                  
                    /* device/links */
                    switch_xml_t xlinks = switch_xml_add_child_d(xdev,"links",0);

                    /* device/links/link (0) */
                    switch_xml_t xlink = switch_xml_add_child_d(xlinks,"link",0);
                    switch_xml_set_attr_d(xlink, "id", "0");
                    switch_xml_set_txt_d(xlink, str_link0.c_str());

                    /* device/links/link (1) */
                    switch_xml_t xlink1 = switch_xml_add_child_d(xlinks,"link",0);
                    switch_xml_set_attr_d(xlink1, "id", "1");
                    switch_xml_set_txt_d(xlink1, str_link0.c_str());
                } break;
            }

            break;
        }
        default:
        {
            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI, stream, FMT("| Board '%d': %-59s |") % device % "No links available.");
                } break;

                case Cli::CONCISE:
                {
                    K::Logger::Logg2(C_MESSAGE, stream, FMT("B%02dLXX:NoLinksAvailable") % device);
                } break;

                case Cli::XML:
                {
                    /* device */
                    xdev = switch_xml_add_child_d(root,"device",0);
                    switch_xml_set_attr_d(xdev,"id",STR(FMT("%d") % device));
                    switch_xml_set_txt_d(xdev,"NoLinksAvailable");
                } break;
            }

            break;
        }
    }
}

void Cli::_KhompShowLinks::showErrors(unsigned int device, OutputType output_type)
{
    if (output_type != Cli::CONCISE && output_type != Cli::XML)
        K::Logger::Logg2(C_CLI, stream, "|-----------------------------------------------------------------------|");

    switch (Globals::k3lutil.physicalLinkCount(device, true))
    {
        case 2:
        {
            K3LUtil::ErrorCountType link0 = Globals::k3lutil.linkErrorCount(
                    device, 0, (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));
            K3LUtil::ErrorCountType link1 = Globals::k3lutil.linkErrorCount(
                    device, 1, (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));

            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI, stream, FMT("|----- Link '0' on board '%d' ------| |----- Link '1' on board '%d' ------|") % device % device);
                    K::Logger::Logg2(C_CLI, stream, "|----------------------------------| |----------------------------------|");
                    K::Logger::Logg2(C_CLI, stream, "|       Error type       | Number  | |       Error type       | Number  |");
                    K::Logger::Logg2(C_CLI, stream, "|----------------------------------| |----------------------------------|");

                    K3LUtil::ErrorCountType::iterator i = link0.begin();
                    K3LUtil::ErrorCountType::iterator j = link1.begin();

                    for (; i != link0.end() && j != link1.end(); i++, j++)
                    {
                        K::Logger::Logg2(C_CLI, stream, FMT("| %22s | %-7d | | %22s | %-7d |")
                           % i->first % i->second % j->first % j->second);
                    }
                } break;

                case Cli::CONCISE:
                {
                    for (K3LUtil::ErrorCountType::iterator i = link0.begin(); i != link0.end(); i++)
                        K::Logger::Logg2(C_CLI, stream, FMT("%d:0:%s:%d") % device % i->first % i->second);

                    for (K3LUtil::ErrorCountType::iterator i = link1.begin(); i != link1.end(); i++)
                        K::Logger::Logg2(C_CLI, stream, FMT("%d:1:%s:%d") % device % i->first % i->second);
                } break;

                case Cli::XML:
                {
                    /* device */
                    xdev = switch_xml_add_child_d(root,"device",0);
                    switch_xml_set_attr_d(xdev,"id",STR(FMT("%d") % device));
                  
                    /* device/errors */
                    switch_xml_t xerrors = switch_xml_add_child_d(xdev,"errors",0);

                    /* device/errors/link (0) */ 
                    switch_xml_t xlinks0 = switch_xml_add_child_d(xerrors,"link",0);
                    switch_xml_set_attr_d(xlinks0,"id","0");

                    for (K3LUtil::ErrorCountType::iterator i = link0.begin(); i != link0.end(); i++)
                    {
                        /* device/errors/link/type */
                        switch_xml_t xtype0 = switch_xml_add_child_d(xlinks0,"type",0);
                        switch_xml_set_txt_d(xtype0,i->first.c_str());

                        /* device/errors/link/number */
                        switch_xml_t xnumber0  = switch_xml_add_child_d(xlinks0,"number",0);
                        switch_xml_set_txt_d(xnumber0,STR(FMT("%d") % i->second));
                    }

                    /* device/errors/link (1) */ 
                    switch_xml_t xlinks1 = switch_xml_add_child_d(xerrors,"link",0);
                    switch_xml_set_attr_d(xlinks1,"id","1");

                    for (K3LUtil::ErrorCountType::iterator i = link1.begin(); i != link1.end(); i++)
                    {
                        /* device/errors/link/type */
                        switch_xml_t xtype1 = switch_xml_add_child_d(xlinks1,"type",0);
                        switch_xml_set_txt_d(xtype1,i->first.c_str());

                        /* device/errors/link/number */
                        switch_xml_t xnumber1  = switch_xml_add_child_d(xlinks1,"number",0);
                        switch_xml_set_txt_d(xnumber1,STR(FMT("%d") % i->second));
                    }
                } break;
            }

            break;
        }

        case 1:
        {
            K3LUtil::ErrorCountType link0 = Globals::k3lutil.linkErrorCount(device, 0, (output_type == Cli::CONCISE ? Verbose::EXACT : Verbose::HUMAN));

            switch(output_type)
            {
                case Cli::VERBOSE:
                {
                    K::Logger::Logg2(C_CLI, stream, FMT("|------------------------ Link '0' on board '%d' ------------------------|") % device);
                    K::Logger::Logg2(C_CLI, stream, "|-----------------------------------------------------------------------|");
                    K::Logger::Logg2(C_CLI, stream, "|                      Error type                      |     Number     |");
                    K::Logger::Logg2(C_CLI, stream, "|-----------------------------------------------------------------------|");

                    for (K3LUtil::ErrorCountType::iterator i = link0.begin(); i != link0.end(); i++)
                        K::Logger::Logg2(C_CLI, stream, FMT("| %52s | %-14d |") % i->first % i->second);
                } break;

                case Cli::CONCISE:
                {
                    for (K3LUtil::ErrorCountType::iterator i = link0.begin(); i != link0.end(); i++)
                        K::Logger::Logg2(C_CLI, stream, FMT("%d:0:%s:%d") % device % i->first % i->second);
                } break;

                case Cli::XML:
                {
                    /* device */
                    xdev = switch_xml_add_child_d(root,"device",0);
                    switch_xml_set_attr_d(xdev,"id",STR(FMT("%d") % device));
                  
                    /* device/errors */
                    switch_xml_t xerrors = switch_xml_add_child_d(xdev,"errors",0);

                    /* device/errors/link (0) */ 
                    switch_xml_t xlinks0 = switch_xml_add_child_d(xerrors,"link",0);
                    switch_xml_set_attr_d(xlinks0,"id","0");

                    for (K3LUtil::ErrorCountType::iterator i = link0.begin(); i != link0.end(); i++)
                    {
                        /* device/errors/link/type */
                        switch_xml_t xtype0 = switch_xml_add_child_d(xlinks0,"type",0);
                        switch_xml_set_txt_d(xtype0,i->first.c_str());

                        /* device/errors/link/number */
                        switch_xml_t xnumber0  = switch_xml_add_child_d(xlinks0,"number",0);
                        switch_xml_set_txt_d(xnumber0,STR(FMT("%d") % i->second));
                    }
                } break;
            }

            break;
        }

        case 0:
        {
            if (output_type != Cli::XML && output_type != Cli::CONCISE)
                K::Logger::Logg2(C_CLI, stream, FMT("|                     No links detected on board %d!                     |") % device);

            break;
        }
    }
}

bool Cli::_KhompShowLinks::execute(int argc, char *argv[])
{
    if(argc < 2 || argc > 5)
    {
        printUsage(stream);
        return false;
    }

    unsigned int dev = UINT_MAX;
    int concise = 0, verbose = 0, as_xml = 0, errors = 0;
    OutputType output_type = Cli::VERBOSE;

    errors = ( (argc > 2) && (!strcasecmp(argv[2],"errors")) ? 1 : 0 );

    concise = ( (argc > (2+errors)) && (!strcasecmp(argv[(2+errors)],"concise")) ? 1 : 0 );
    verbose = ( (argc > (2+errors)) && (!strcasecmp(argv[(2+errors)],"verbose")) ? 1 : 0 );
    as_xml  = ( (argc > (2+errors)) && (!strcasecmp(argv[(2+errors)],"xml"))     ? 1 : 0 );

    if(argc > (2+errors+concise+verbose+as_xml))
    {
        dev = atoi(argv[(2+errors+concise+verbose+as_xml)]);

        if (!Globals::k3lapi.valid_device(dev))
        {
            K::Logger::Logg2(C_CLI, stream, "ERROR: no such device!");
            return false;
        }
    }


    if (!concise && !as_xml)
    {
        K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------");
        if(!errors)
        {
            K::Logger::Logg2(C_CLI, stream, "|--------------------------- Khomp Links List ---------------------------|");
        }
        else
        {
            K::Logger::Logg2(C_CLI, stream, "|-------------------- Khomp Errors Counters on Links -------------------|");
        }
    }

    if(concise) output_type = Cli::CONCISE;
    if(as_xml)
    {
        output_type = Cli::XML;
        createRoot("links");
    }

    if (dev == UINT_MAX)
    {
        for (dev = 0; dev < Globals::k3lapi.device_count(); dev++)
        {
            if(!errors)
            {
                showLinks (dev, output_type);
            }
            else
            {
                showErrors(dev, output_type);
            }
        }
    }
    else
    {
        if(!errors)
        {
            showLinks (dev, output_type);
        }
        else
        {
            showErrors(dev, output_type);
        }
    }

    if (!concise && !as_xml)
    {
        K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------");
    }

    if (as_xml) 
    {
        printXMLOutput(stream);
        clearRoot();
    }

    return true;
}

void Cli::_KhompClearLinks::clearLink(unsigned int device, unsigned int link)
{
    try
    {
        Globals::k3lapi.command(device, link, CM_CLEAR_LINK_ERROR_COUNTER);
    }
    catch(K3LAPI::failed_command & e)
    {
        K::Logger::Logg2(C_CLI, stream, 
                FMT("ERROR: Command has failed with error '%s'") 
                % Verbose::status((KLibraryStatus)e.rc).c_str());
    }
}

bool Cli::_KhompClearLinks::execute(int argc, char *argv[])
{
    if(argc < 2 || argc > 4)
    {
        printUsage(stream);
        return false;
    }

    unsigned int dev = UINT_MAX;
    unsigned int obj = UINT_MAX;    

    if(argc > 2)
    {
        dev = atoi(argv[2]);

        if (!Globals::k3lapi.valid_device(dev))
        {
            K::Logger::Logg2(C_CLI, stream, 
                    FMT("ERROR: no such device %d!") % dev);
            return false;
        }

        if(argc > 3)
        {
            obj = atoi(argv[3]);

            if(!Globals::k3lapi.valid_link(dev, obj))
            {
                K::Logger::Logg2(C_CLI, stream, 
                      FMT("ERROR: No such link %d on device %d!") % obj % dev);
                return false;
            }
        }
    }

    if(dev == UINT_MAX)
    {
        K::Logger::Logg2(C_CLI, stream, 
                "NOTICE: Reseting error count of all links...");

        for (unsigned int d = 0; d < Globals::k3lapi.device_count(); d++)
        {
            unsigned int link_count = Globals::k3lutil.physicalLinkCount(d, true);

            for (unsigned int o = 0; o < link_count; o++)
                clearLink(d, o);
        }
    }
    else
    {
        if (obj == UINT_MAX)
        {
            K::Logger::Logg2(C_CLI, stream, 
                FMT("NOTICE: Reseting error count of all links on device %d...")
                % dev);
            unsigned int link_count = Globals::k3lutil.physicalLinkCount(dev, true);

            for (unsigned int o = 0; o < link_count; o++)
                clearLink(dev, o);
        }
        else
        {
            K::Logger::Logg2(C_CLI, stream, 
                FMT("NOTICE: Reseting error count of link %d on device %d...") 
                % obj % dev);

            clearLink(dev, obj);
        }
    }

    return true;
}

bool Cli::_KhompClearStatistics::execute(int argc, char *argv[])
{
    if(argc < 2 || argc > 4)
    {
        printUsage(stream);
        return false;
    }

    try
    {

    unsigned int dev = UINT_MAX;
    unsigned int obj = UINT_MAX;    

    if(argc > 2)
    {
        dev = atoi(argv[2]);

        if (!Globals::k3lapi.valid_device(dev))
        {
            K::Logger::Logg2(C_CLI, stream, 
                    FMT("ERROR: no such device %d!") % dev);
            return false;
        }

        if(argc > 3)
        {
            obj = atoi(argv[3]);


            if(!Globals::k3lapi.valid_channel(dev, obj))
            {
                K::Logger::Logg2(C_CLI, stream, 
                      FMT("ERROR: No such channel %d on device %d!") % obj % dev);
                return false;
            }
        }
    }

    if(dev == UINT_MAX)
    {
        K::Logger::Logg2(C_CLI, stream, "NOTICE: Reseting statistics of all channels...");

        for (unsigned int d = 0; d < Globals::k3lapi.device_count(); d++)
        {
            for (unsigned int o = 0; o < Globals::k3lapi.channel_count(d); o++) 
                Board::get(d, o)->clearStatistics();
        }
    }
    else
    {
        if (obj == UINT_MAX)
        {
            K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Reseting statistics of all channels from board %d...") % dev);
            
            for (unsigned int o = 0; o < Globals::k3lapi.channel_count(dev); o++) 
                Board::get(dev, o)->clearStatistics();
        }
        else
        {
            K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Reseting statistics of channel %d from board %d") % obj % dev);
            Board::get(dev, obj)->clearStatistics();
        }
    }

    }
    catch (K3LAPITraits::invalid_channel & err)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
        return false;
    }

    return true;
}

bool Cli::_KhompDumpConfig::execute(int argc, char *argv[])
{
    if (argc != 2) 
    {
        printUsage(stream);
        return false;
    }

    const Config::StringSet opts = Globals::options.options();

    K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------");
    K::Logger::Logg2(C_CLI, stream, "|--------------------------- Khomp Options Dump -------------------------|");
    K::Logger::Logg2(C_CLI, stream, "|------------------------------------------------------------------------|");

    for (Config::StringSet::const_iterator itr = opts.begin(); itr != opts.end(); ++itr)
    {   try
        {
            if(removeUnavaible((*itr))) continue;                
            K::Logger::Logg2(C_CLI, stream, FMT("| %-24s => %42s |")
                    % (*itr) % Globals::options.get(&(Opt::_options), (*itr)));
        }
        catch(Config::EmptyValue &e)
        {
            K::Logger::Logg(C_ERROR, FMT("%s (%s)") % e.what() % (*itr));
        }
    }    

    K::Logger::Logg2(C_CLI, stream, " ------------------------------------------------------------------------");

    return true;
}

void Cli::_KhompResetLinks::resetLink(unsigned int device, unsigned int link)
{
    try
    {
        Globals::k3lapi.command(device, link, CM_RESET_LINK);
    }
    catch(K3LAPI::failed_command & e)
    {
        K::Logger::Logg2(C_CLI, stream, 
                FMT("ERROR: Command has failed with error '%s'") 
                % Verbose::status((KLibraryStatus)e.rc).c_str());
    }
}

bool Cli::_KhompResetLinks::execute(int argc, char *argv[])
{
    if(argc < 2 || argc > 4)
    {
        printUsage(stream);
        return false;
    }

    unsigned int dev = UINT_MAX;
    unsigned int obj = UINT_MAX;    

    if(argc > 2)
    {
        dev = atoi(argv[2]);

        if (!Globals::k3lapi.valid_device(dev))
        {
            K::Logger::Logg2(C_CLI, stream, 
                    FMT("ERROR: no such device %d!") % dev);
            return false;
        }

        if(argc > 3)
        {
            obj = atoi(argv[3]);

            if(!Globals::k3lapi.valid_link(dev, obj))
            {
                K::Logger::Logg2(C_CLI, stream, 
                      FMT("ERROR: No such link %d on device %d!") % obj % dev);
                return false;
            }
        }
    }

    if(dev == UINT_MAX)
    {
        K::Logger::Logg2(C_CLI, stream, 
                "NOTICE: Reseting all links...");

        for (unsigned int d = 0; d < Globals::k3lapi.device_count(); d++)
        {
            unsigned int link_count = Globals::k3lutil.physicalLinkCount(d, true);

            for (unsigned int o = 0; o < link_count; o++)
                resetLink(d, o);
        }
    }
    else
    {
        if (obj == UINT_MAX)
        {
            K::Logger::Logg2(C_CLI, stream, 
                FMT("NOTICE: Reseting all links on device %d...")
                % dev);
            unsigned int link_count = Globals::k3lutil.physicalLinkCount(dev, true);

            for (unsigned int o = 0; o < link_count; o++)
                resetLink(dev, o);
        }
        else
        {
            K::Logger::Logg2(C_CLI, stream, 
                FMT("NOTICE: Reseting link %d on device %d...") 
                % obj % dev);

            resetLink(dev, obj);
        }
    }

    return true;
}

bool Cli::_KhompSMS::execute(int argc, char *argv[])
{
    if(argc < 4)
    {
        printUsage(stream);
        return false;
    }

    std::string devs(argv[1]);
    std::string numb(argv[2]);
    std::string mesg(argv[3]);

    for (unsigned int i = 4; i < (unsigned int)argc; i++)
    {
        mesg += " ";
        mesg += argv[i];
    }

    Board::KhompPvt * pvt = NULL;

    bool enable_retry = false;

    size_t pos = numb.find('r');
    if (pos != std::string::npos)
    {
        numb.erase(pos,1);
        enable_retry = true;
    }

    std::string begin;

    if(devs[0] != 'b' && devs[0] != 'B')
    {
        begin = "b";
    }

    std::string complete = begin + devs + "/" + numb + "/" + mesg;
    int cause = (int)SWITCH_CAUSE_NONE;

    try
    {
        for (unsigned int ntry = 0; ntry < 15; ++ntry)
        {
            ScopedAllocLock alloc_lock;

            pvt = processSMSString(complete.c_str(), &cause);

            /* NOTE: go directly to the pvt check below! */
            if (!enable_retry || pvt)
                break;

            alloc_lock.unlock();

            K::Logger::Logg2(C_CLI, stream, FMT("WARNING: '%s': No available channel, trying again...") % devs);
            usleep(2500000);
        }

        if (!pvt)
        {
            K::Logger::Logg2(C_CLI, stream, FMT("ERROR: '%s': No available channel %s") % devs % (enable_retry ? "after 15 retries, giving up!" : ""));
            return false;
        }

        ScopedPvtLock lock(pvt);

        if(!pvt->application(SMS_SEND, NULL, complete.c_str()))
        {
            K::Logger::Logg2(C_CLI, stream, "ERROR: Message could not be sent");
            return false;
        }

    }
    catch(ScopedLockFailed & err)
    {
        if(err._fail == ScopedLockFailed::ALLOC_FAILED)
        {
            K::Logger::Logg2(C_CLI, stream, "ERROR: unable to global alloc lock");
        }
        else
        {
            K::Logger::Logg2(C_CLI, stream, FMT("ERROR: unable to lock: %s!") % err._msg.c_str());
        }
        return false;
    }
        
    K::Logger::Logg2(C_CLI, stream, "Message sent successfully!");

    return true;
}

bool Cli::_KhompLogConsole::execute(int argc, char *argv[])
{
    if (argc < 3)
    {
        printUsage(stream);
        return false;
    }

    bool invert = false;
    bool unique = false;

    std::string extra(argv[2]);

    unsigned int total_args = argc - 2; /* remove "khomp log console" */

    if (extra == "no")
    {    
        invert = true;
        --total_args;
    }    
    else if (extra == "just")
    {    
        unique = true;
        --total_args;
    }    

    unsigned int first_args = argc - total_args;

    std::string options;

    for (unsigned int i = first_args; i < (unsigned int)argc; i++) 
    {    
        options += argv[i];
        options += ","; 
    }    

    K::Logger::processLogConsole(stream, options, invert, unique);
    return true;
}

bool Cli::_KhompLogDisk::execute(int argc, char *argv[])
{
    if (argc < 3)
    {
        printUsage(stream);
        return false;
    }

    bool invert = false;
    bool unique = false;

    std::string extra(argv[2]);

    unsigned int total_args = argc - 2; /* remove "khomp log disk" */

    if (extra == "no")
    {
        invert = true;
        --total_args;
    }
    else if (extra == "just")
    {
        unique = true;
        --total_args;
    }

    unsigned int first_args = argc - total_args;

    std::string options;

    for (unsigned int i = first_args; i < (unsigned int)argc; i++)
    {
        options += argv[i];
        options += ",";
    }

    K::Logger::processLogDisk(stream, options, invert, unique);
    return true;
}

bool Cli::_KhompLogStatus::execute(int argc, char *argv[])
{
    if(argc != 2)
    {
        printUsage(stream);
        return false;
    }

    Strings::Merger m1;

    bool flag1_errors   = K::Logger::Logg.classe(C_ERROR).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_warnings = K::Logger::Logg.classe(C_WARNING).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_messages = K::Logger::Logg.classe(C_MESSAGE).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_events   = K::Logger::Logg.classe(C_EVENT).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_commands = K::Logger::Logg.classe(C_COMMAND).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_audio    = K::Logger::Logg.classe(C_AUDIO_EV).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_modem    = K::Logger::Logg.classe(C_MODEM_EV).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_links    = K::Logger::Logg.classe(C_LINK_STT).get(O_CONSOLE, K::LogManager::Option::ENABLED);
    bool flag1_cas      = K::Logger::Logg.classe(C_CAS_MSGS).get(O_CONSOLE, K::LogManager::Option::ENABLED);

    if (flag1_errors)   m1.add("errors");
    if (flag1_warnings) m1.add("warnings");
    if (flag1_messages) m1.add("messages");
    if (flag1_events)   m1.add("events");
    if (flag1_commands) m1.add("commands");
    if (flag1_audio)    m1.add("audio");
    if (flag1_modem)    m1.add("modem");
    if (flag1_links)    m1.add("link");
    if (flag1_cas)      m1.add("cas");

    K::Logger::Logg2(C_CLI, stream, "             ");

    if (!m1.empty()) K::Logger::Logg2(C_CLI, stream, FMT("Enabled console messages: %s.") % m1.merge(", "));
    else /* ----- */ K::Logger::Logg2(C_CLI, stream, "There are no console messages enabled.");

    bool flag2_errors    = K::Logger::Logg.classe(C_ERROR).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_warnings  = K::Logger::Logg.classe(C_WARNING).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_messages  = K::Logger::Logg.classe(C_MESSAGE).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_events    = K::Logger::Logg.classe(C_EVENT).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_commands  = K::Logger::Logg.classe(C_COMMAND).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_audio     = K::Logger::Logg.classe(C_AUDIO_EV).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_modem     = K::Logger::Logg.classe(C_MODEM_EV).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_links     = K::Logger::Logg.classe(C_LINK_STT).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_cas       = K::Logger::Logg.classe(C_CAS_MSGS).get(O_GENERIC, K::LogManager::Option::ENABLED);
    bool flag2_functions = K::Logger::Logg.classe(C_DBG_FUNC).enabled();
    bool flag2_threads   = K::Logger::Logg.classe(C_DBG_THRD).enabled();
    bool flag2_locks     = K::Logger::Logg.classe(C_DBG_LOCK).enabled();
    bool flag2_streams   = K::Logger::Logg.classe(C_DBG_STRM).enabled();

    Strings::Merger m2;

    if (flag2_errors)    m2.add("errors");
    if (flag2_warnings)  m2.add("warnings");
    if (flag2_messages)  m2.add("messages");
    if (flag2_events)    m2.add("events");
    if (flag2_commands)  m2.add("commands");
    if (flag2_audio)     m2.add("audio");
    if (flag2_modem)     m2.add("modem");
    if (flag2_links)     m2.add("link");
    if (flag2_cas)       m2.add("cas");
    if (flag2_functions) m2.add("functions");
    if (flag2_threads)   m2.add("threads");
    if (flag2_locks)     m2.add("locks");
    if (flag2_streams)   m2.add("streams");

    if (!m2.empty()) K::Logger::Logg2(C_CLI, stream, FMT("Enabled log-on-disk messages: %s.") % m2.merge(", "));
    else /* ----- */ K::Logger::Logg2(C_CLI, stream, "There are no log-on-disk messages enabled.");
   
    if (Globals::flag_trace_rdsi) K::Logger::Logg2(C_CLI, stream, "The ISDN (RDSI) low-level tracing is enabled.");

    K::Logger::Logg2(C_CLI, stream, "             ");

    return true;
}

bool Cli::_KhompLogRotate::execute(int argc, char *argv[])
{
    if (argc != 2)
    {
        printUsage(stream);
        return false;
    }

    if (!K::Logger::rotate())
    {    
        return false;
    }    

    return true;
}

bool Cli::_KhompLogTraceK3L::execute(int argc, char *argv[])
{
    if (argc != 4)
    {
        printUsage(stream);
        return false;
    }

    std::string str_on("on");
    std::string str_off("off");

    bool value = false;

         if (str_on  == argv[3]) value = true;
    else if (str_off == argv[3]) value = false;
    else 
    {    
        K::Logger::Logg2(C_CLI, stream, "ERROR: Please use 'on' or 'off' to enable or disable.");
        return false;
    }    

    K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: %sbling k3l debug messages.") % (value ? "Ena" : "Disa"));

    Logfile logfile;

    K::LogConfig::set(logfile, "K3L", "Value",        value);
    K::LogConfig::set(logfile, "K3L", "CallProgress", value);
    K::LogConfig::set(logfile, "K3L", "CallAnalyzer", value);
    K::LogConfig::set(logfile, "K3L", "CadenceRecog", value);
    K::LogConfig::set(logfile, "K3L", "CallControl",  value);
    K::LogConfig::set(logfile, "K3L", "Fax",          value);

    if (K::LogConfig::commit(logfile))
    {    
        try
        {
            Globals::k3lapi.command(-1, -1, CM_LOG_UPDATE);
        }
        catch(...)
        {
            LOG(ERROR,"Error while send command CM_LOG_UPDATE");
        }
    } 

    return true;
}

bool Cli::_KhompLogTraceISDN::execute(int argc, char *argv[])
{
    if (argc < 4)
    {
        printUsage(stream);
        return false;
    }

    bool active = true;

    bool change_lapd = false;
    bool change_q931 = false;

    std::string what;

    for (unsigned int i = 3; i < (unsigned int)argc; i++)
    {
        what += argv[i];
        what += ",";
    }

    Strings::vector_type values;
    Strings::tokenize(what, values, ",");

    for (Strings::vector_type::iterator i = values.begin(); i != values.end(); i++)
    {
        if ((*i) == "q931") change_q931 = true;
        if ((*i) == "lapd") change_lapd = true;
        if ((*i) == "off")  active = false;
    }

    Logfile logfile;

    K::LogConfig::set(logfile, "ISDN", "Value", active);

    if (change_lapd || !active)
        K::LogConfig::set(logfile, "ISDN", "Lapd", active);

    if (change_q931 || !active)
        K::LogConfig::set(logfile, "ISDN", "Q931", active);

    if (K::LogConfig::commit(logfile))
    {
        try
        {
            Globals::k3lapi.command(-1, -1, CM_LOG_UPDATE);

            if (active)
            {
                K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Activating the following ISDN debug option(s): %s") % what);
                Globals::flag_trace_rdsi = true;
            }
            else
            {
                K::Logger::Logg2(C_CLI, stream, "NOTICE: Deactivating ISDN debug options");
                Globals::flag_trace_rdsi = false;
            }
        }
        catch(...)
        {
            if (active)
                K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to activate the following ISDN debug option(s): %s") % what);
            else
                K::Logger::Logg2(C_CLI, stream, "ERROR: Unable to deactivate ISDN debug options");
        }

        return false;
    }

    return true;
}

bool Cli::_KhompLogTraceR2::execute(int argc, char *argv[])
{
    if (argc != 4)
    {
        printUsage(stream);
        return false;
    }

    try
    {
        std::string str_off("off");
        std::string str_on("on" );

        bool enable = false;

        if (str_on   == argv[3]) enable = true;
        else if (str_off  == argv[3]) enable = false;

        if (enable)
        {
            K::Logger::Logg2(C_CLI, stream, "NOTICE: All channels of all devices will be monitored!" );
        }
        else
        {
            K::Logger::Logg2(C_CLI, stream, "NOTICE: Deactivating R2 debug options" );
        }

        Logfile logfile;

        K::LogConfig::set(logfile, "R2", "Value",     enable);
        K::LogConfig::set(logfile, "R2", "Signaling", enable);
        K::LogConfig::set(logfile, "R2", "States",    enable);

        if (K::LogConfig::commit(logfile))
        {
            try
            {
                Globals::k3lapi.command(-1, -1, CM_LOG_UPDATE);
            }
            catch(...)
            {
                LOG(ERROR,"Error while send command CM_LOG_UPDATE");
            }
        }
    }
    catch (Strings::invalid_value e)
    {
        K::Logger::Logg2(C_CLI, stream, "ERROR: Invalid numeric value in arguments.");
        return false;
    }

    return true;
}

bool Cli::_KhompGet::execute(int argc, char *argv[])
{
    if (argc < 2) 
    {
        printUsage(stream);
        return false;
    }

    std::string arg(argv[1]);

    try  
    {    
       std::string res = Globals::options.get(&Opt::_options, (const char*) argv[1]);
       K::Logger::Logg2(C_CLI, stream, FMT("Result for command %s is %s.") % std::string(argv[1]) % res);
    }catch(Config::Failure &e){ 
       K::Logger::Logg2(C_CLI, stream, e.what());
    }    

    return true;
}

bool Cli::_KhompSet::execute(int argc, char *argv[])
{
    if (argc < 3) 
    {
        printUsage(stream);
        return false;
    }

    std::string args;

    const unsigned int first = 2; 

    for (unsigned int i = first; i < (unsigned int)argc; i++) 
    {    
        if (i != first) { args += " "; }

        args += argv[i];
    }    

    try  
    {    
        Globals::options.process(&Opt::_options, (const char *) argv[1], (const char *) args.c_str());
        const Config::Options::Messages msgs = Globals::options.commit(&Opt::_options, (const char *)argv[1]);

        for (Config::Options::Messages::const_iterator i = msgs.begin(); i != msgs.end(); ++i) 
        {
            K::Logger::Logg2(C_ERROR, stream, FMT("%s.") % (*i));
        }

        K::Logger::Logg2(C_CLI, stream, FMT("Setting %s for value %s") % argv[1] % argv[2]);
    }    
    catch (Config::Failure &e)
    {    
        K::Logger::Logg2(C_ERROR,stream, FMT("config processing error: %s.") % e.what());
    }    

    return true;
}

bool Cli::_KhompRevision::execute(int argc, char *argv[])
{
#ifndef MOD_KHOMP_VERSION
#define MOD_KHOMP_VERSION "unknown"
#endif

#ifndef SWITCH_VERSION_FULL
#define SWITCH_VERSION_FULL "unknown"
#endif

    std::string khomp_endpoint_rev(MOD_KHOMP_VERSION);
    std::string freeswitch_rev(SWITCH_VERSION_FULL);

    K::Logger::Logg2(C_CLI, stream, FMT("Khomp Endpoint - %s") % khomp_endpoint_rev);
    K::Logger::Logg2(C_CLI, stream, FMT("FreeSWITCH - %s") % freeswitch_rev);
    return true;
}

bool Cli::_KhompSendCommand::execute(int argc, char *argv[])
{
    if (argc < 5 || argc > 6)
    {
        printUsage(stream);
        return false;
    }

    unsigned int dev = atoi (argv[2]);

    if (dev >= Globals::k3lapi.device_count())
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: No such device: %d!") % dev);
        return false;
    }

    unsigned int num = atoi (argv[4]);

    if (num >= 256)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Invalid command number: %d!") % num);
        return false;
    }

    unsigned int obj = atoi (argv[3]);

    try
    {
        switch (argc)
        {
            case 5:    Globals::k3lapi.command(dev,obj, num);         break;
            case 6:    Globals::k3lapi.command(dev,obj, num,argv[5]); break;
            default:   /*               what-a-hell ?!             */ break;
        }
    }
    catch(K3LAPI::failed_command &e)
    {
        if (K::Logger::Logg.classe(C_WARNING).enabled())
        {   
            LOG(WARNING, FMT("Command '%s' has failed with error '%s'.")
                % Verbose::commandName(e.code) % Verbose::status((KLibraryStatus)e.rc));
        }   

        return false;
    }

    return true;
}

bool Cli::_KhompSendRawCommand::execute(int argc, char *argv[])
{
    if (argc < 6) 
    {
        printUsage(stream);
        return false;
    }

    unsigned int dev = atoi (argv[3]);

    if (dev >= Globals::k3lapi.device_count())
    {    
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: No such device: %d!") % dev);
        return false;
    }    

    unsigned int dsp = atoi (argv[4]);

    if (dsp >= 2)
    {    
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Invalid DSP number: %d!") % dsp);
        return false;
    }    

    const unsigned int base = 5;
    char commands[(argc - base)];

    for (int i = base, j = 0; i < argc; i++, j++) 
    {    
        if (sscanf(argv[i], "%hhx", &(commands[j])) != 1)
        {    
            K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Invalid hexadecimal sequence: '%s'!") % argv[i]);
            return false;
        }    
    }    

    try
    {
        Globals::k3lapi.raw_command(dev, dsp, commands, (argc - base));
    }
    catch(K3LAPI::failed_raw_command &e)
    {
        if (K::Logger::Logg.classe(C_WARNING).enabled())
        {   
            LOG(WARNING, FMT("(dev=%d,dsp=%d): Raw command '%s' has failed with error '%s'.")
            % e.dev 
            % e.dsp 
            % Strings::hexadecimal(std::string((char*) commands,(argc - base))) 
            % Verbose::status((KLibraryStatus)e.rc));
        }   

        return false;
    }

   return true; 
}

bool Cli::_KhompSelectSim::execute(int argc, char *argv[])
{
    if(argc != 5)
    {
        printUsage(stream);
        return false;
    }

    std::string dev_str(argv[2]);
    std::string obj_str(argv[3]);
    std::string num_str(argv[4]);

    try
    {
        unsigned int dev = Strings::tolong(dev_str);
        unsigned int obj = Strings::tolong(obj_str);

        if (!Globals::k3lapi.valid_device(dev))
        {
            K::Logger::Logg2(C_CLI, stream, 
                    FMT("ERROR: no such device %d!") % dev);
            return false;
        }

        if (!Globals::k3lapi.valid_channel(dev, obj))
        {
            K::Logger::Logg2(C_CLI, stream,
                    FMT("ERROR: No such channel %d on device %d!") % obj % dev);
            return false;
        }

        /* just check for validity */
        (void)Strings::tolong(num_str);


        if(!Board::get(dev, obj)->application(SELECT_SIM_CARD, NULL, num_str.c_str()))
        {
            K::Logger::Logg2(C_CLI, stream, "ERROR: Unable to select sim card"); 
            return false;
        }
    }
    catch (Strings::invalid_value & e)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Invalid number '%s'!") % e.value());
        return false;
    }
    catch (K3LAPITraits::invalid_channel & err)
    {
        K::Logger::Logg2(C_CLI, stream, FMT("ERROR: Unable to find channel %d on device %d!") % err.object % err.device );
        return false;
    }
    catch(...)
    {
        K::Logger::Logg2(C_CLI, stream, "ERROR: Unable to select sim card"); 
        return false;
    }

    return true;
}

bool Cli::_KhompKommuterOnOff::execute(int argc, char *argv[])
{
    if(argc != 2)
    {
        printUsage(stream);
        return false;
    }

    switch (Board::kommuter._kommuter_count)
    {
        case -1: K::Logger::Logg2(C_CLI, stream, "ERROR: libkwd.so required for kommuter could not be found." ); return false;
        case 0:  K::Logger::Logg2(C_CLI, stream, "ERROR: none Kommuter was found on the system." );              return false;
    }
    
    if (Opt::_options._kommuter_activation() == "auto")
    {
        K::Logger::Logg2(C_CLI, stream, "ERROR: Kommuter is set to be started automatically by kommuter-activation configuration.");
        return false;
    }
    
    bool on_off = ARG_CMP(1, "on") ? true : false;

    int ret = 0;
    if (on_off)
    {
        int timeout = Opt::_options._kommuter_timeout();
        K::Logger::Logg2(C_CLI, stream, FMT("NOTICE: Activating Kommuters with timeout of %d seconds .") % timeout);
        bool start_timer = false;

        std::string param= STG(FMT("timeout=%d") % timeout);

        for (int kommuter = 0; kommuter < Board::kommuter._kommuter_count; kommuter++)
        {
            try
            {
                Globals::k3lapi.command(-1, kommuter, CM_START_WATCHDOG, (char*) param.c_str());
                start_timer = true;
            }
            catch(K3LAPI::failed_command & e)
            {
                switch(e.rc)
                {
                case ksInvalidParams:
                    K::Logger::Logg2(C_CLI,stream,FMT("ERROR: invalid timeout '%d' for Kommuter device '%d' timeout. Mininum is 0, maximum is 255.") 
                % timeout % kommuter);
                    break;
                default:
                    K::Logger::Logg2(C_CLI,stream,FMT("ERROR: could not start the kommuter device number '%d'.") % kommuter);
                    break;
                }
            } 
            catch(...)
            {
                K::Logger::Logg2(C_CLI, stream, FMT("ERROR: could not start the Kommuter device number '%d'.") % kommuter);
            }
        }

        if (timeout == 0)
        {
            DBG(FUNC, D("kommuter watchdog timer not created because timeout is 0."));
            return false;
        }        
        
        if (start_timer)
        {    
            if (!Globals::global_timer)
            {    
                K::Logger::Logg2(C_CLI, stream , "Error creating the timer for kommuter.");
                return true;
            }    
            else if (Board::kommuter._kwtd_timer_on)
            {    
                Globals::global_timer->restart( Board::kommuter._kwtd_timer_index, true );
                DBG(FUNC, D("kommuter watchdog timer restarted."));
            }
            else
            {
                Board::kommuter._kwtd_timer_index = Globals::global_timer->add(((timeout < 5) ? (timeout*500) : 2000), &Kommuter::wtdKickTimer);
                Board::kommuter._kwtd_timer_on = true;
                DBG(FUNC, D("kommuter watchdog timer created and started."));
            }
        }    
    }    
    else 
    {    
        K::Logger::Logg2(C_CLI, stream, "NOTICE: Deactivating Kommuters.");

        if (Board::kommuter._kwtd_timer_on)
        {    
            Globals::global_timer->del( Board::kommuter._kwtd_timer_index);
            Board::kommuter._kwtd_timer_on = false;
        }    

        for(int kommuter = 0; kommuter < Board::kommuter._kommuter_count; kommuter++)
        {    
            try
            {
                Globals::k3lapi.command(-1, kommuter, CM_STOP_WATCHDOG);
                return true;
            }
            catch(K3LAPI::failed_command & e)
            {
                K::Logger::Logg2(C_CLI,stream,FMT("ERROR: Kommuter device '%d' was not initialized.") % kommuter);
            } 
            catch(...)
            {
                K::Logger::Logg2(C_CLI, stream,FMT("ERROR: could not disable kommuter device '%d' for some unknow reason.") % kommuter);
            }
        }    
    }

    return false;
}

bool Cli::_KhompKommuterCount::execute(int argc, char *argv[])
{
    if(argc != 2)
    {
        printUsage(stream);
        return false;
    }

    if(Board::kommuter._kommuter_count == -1)
    {
        K::Logger::Logg2(C_CLI, stream, 
                "ERROR: libkwd.so required for kommuter could not be found." );
        return false;
    }

    K::Logger::Logg2(C_CLI, stream, 
            FMT("Kommuter devices detected = [%d]") 
            % Board::kommuter._kommuter_count);

    return true;
}

