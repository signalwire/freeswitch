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

#include "opt.h"
#include "globals.h"
#include "defs.h"
#include "logger.h"
#include "spec.h"

#include <function.hpp>

Options               Opt::_options;
CadencesMapType       Opt::_cadences;
GroupToDestMapType    Opt::_groups;
OrigToDestMapType     Opt::_fxs_hotline;
BoardToOrigMapType    Opt::_fxs_orig_base;
BranchToOptMapType    Opt::_branch_options;
BranchToObjectMapType Opt::_fxs_branch_map;

/* not beautiful, should think of something! */
#define FUNCTION_VALUE(x) reinterpret_cast< Config::FunctionValue Options::* >( x )

void Opt::initialize(void) 
{ 
    Globals::options.add(Config::Option("debug",    &Options::_debug,    false));
    Globals::options.add(Config::Option("dialplan", &Options::_dialplan, "XML"));
    Globals::options.add(Config::Option("context",  &Options::_context,  "default"));

	Globals::options.add(Config::Option("echo-canceller",      &Options::_echo_canceller,      true));
	Globals::options.add(Config::Option("auto-gain-control",   &Options::_auto_gain_control,   true));
	Globals::options.add(Config::Option("out-of-band-dtmfs",   &Options::_out_of_band_dtmfs,   true));
	Globals::options.add(Config::Option("suppression-delay",   &Options::_suppression_delay,   true));
	Globals::options.add(Config::Option("pulse-forwarding",    &Options::_pulse_forwarding,    false));
	Globals::options.add(Config::Option("native-bridge",       &Options::_native_bridge,       true));
	Globals::options.add(Config::Option("recording",           &Options::_recording,           true));
	Globals::options.add(Config::Option("has-ctbus",           &Options::_has_ctbus,           false));
	Globals::options.add(Config::Option("fxs-bina",            &Options::_fxs_bina,            true));
	Globals::options.add(Config::Option("fxs-sharp-dial",      &Options::_fxs_sharp_dial,      true));
	Globals::options.add(Config::Option("drop-collect-call",   &Options::_drop_collect_call,   false));
	Globals::options.add(Config::Option("ignore-letter-dtmfs", &Options::_ignore_letter_dtmfs, true));
	Globals::options.add(Config::Option("optimize-audio-path", &Options::_optimize_audio_path, false));

    Globals::options.add(Config::Option("fxo-send-pre-audio",     &Options::_fxo_send_pre_audio,  true));
    Globals::options.add(Config::Option("fxo-busy-disconnection", &Options::_fxo_busy_disconnection, 1250u, 50u, 90000u));

    Globals::options.add(Config::Option("auto-fax-adjustment",    &Options::_auto_fax_adjustment,    true));
    Globals::options.add(Config::Option("fax-adjustment-timeout", &Options::_fax_adjustment_timeout, 30u, 3u, 9999u));

    Globals::options.add(Config::Option("r2-strict-behaviour", &Options::_r2_strict_behaviour, false));
    Globals::options.add(Config::Option("r2-preconnect-wait",  &Options::_r2_preconnect_wait,  250u, 25u, 500u));

    Globals::options.add(Config::Option("fxs-digit-timeout",    &Options::_fxs_digit_timeout,   7u,  1u, 30u));
    Globals::options.add(Config::Option("transferdigittimeout", &Options::_transferdigittimeout, 3000u, 0u, 90000u));

    Globals::options.add(Config::Option("atxfer",    &Options::_atxfer, ""));
    Globals::options.add(Config::Option("blindxfer", &Options::_blindxfer, ""));

    Globals::options.add(Config::Option("flash-to-digits", &Options::_flash, "*1"));

    Globals::options.add(Config::Option("delay-ringback-co",  &Options::_ringback_co_delay,  1500u, 0u, 999000u)); 
    Globals::options.add(Config::Option("delay-ringback-pbx", &Options::_ringback_pbx_delay, 2500u, 0u, 999000u));
    
    Globals::options.add(Config::Option("disconnect-delay", &Options::_disconnect_delay, 0u, 0u, 100000u)); 

    Globals::options.add(Config::Option("input-volume",  &Options::_input_volume, 0, -10, 10));
    Globals::options.add(Config::Option("output-volume", &Options::_output_volume, 0, -10, 10));

    Globals::options.add(Config::Option("fxs-co-dialtone", 
        FUNCTION_VALUE(&Options::_fxs_co_dialtone), ""));

    Globals::options.add(Config::Option("log-to-disk",   
        FUNCTION_VALUE(&Options::_log_disk_option),    "standard", false));

    Globals::options.add(Config::Option("callgroup", &Options::_callgroup,   "0"));

    Globals::options.add(Config::Option("pickupgroup", &Options::_pickupgroup, "0"));
    
    Globals::options.add(Config::Option("log-to-console",
        FUNCTION_VALUE(&Options::_log_console_option), "standard", false));

    Globals::options.add(Config::Option("trace",
        FUNCTION_VALUE(&Options::_log_trace_option), "", false));

    Globals::options.add(Config::Option("record-prefix", 
        FUNCTION_VALUE(&Options::_record_prefix), "/var/spool/freeswitch/monitor/"));
    
    Globals::options.add(Config::Option("fxs-global-orig", &Options::_fxs_global_orig_base,  "0"));

    Globals::options.add(Config::Option("language", &Options::_global_language, ""));
    Globals::options.add(Config::Option("mohclass", &Options::_global_mohclass, ""));

    Globals::options.add(Config::Option("context-fxo",          &Options::_context_fxo,       "khomp-DD-CC"));
    Globals::options.add(Config::Option("context-fxo-alt",      &Options::_context2_fxo,      "khomp-DD"));
    Globals::options.add(Config::Option("context-fxs",          &Options::_context_fxs,       "khomp-DD-CC"));
    Globals::options.add(Config::Option("context-fxs-alt",      &Options::_context2_fxs,      "khomp-DD"));
    Globals::options.add(Config::Option("context-gsm-call",     &Options::_context_gsm_call,  "khomp-DD-CC"));
    Globals::options.add(Config::Option("context-gsm-call-alt", &Options::_context2_gsm_call, "khomp-DD"));
    Globals::options.add(Config::Option("context-gsm-sms",      &Options::_context_gsm_sms,   "khomp-sms-DD-CC"));
    Globals::options.add(Config::Option("context-digital",      &Options::_context_digital,   "khomp-DD-LL"));
    Globals::options.add(Config::Option("context-pr",           &Options::_context_pr,        "khomp-DD-CC"));

    Globals::options.add(Config::Option("accountcode", &Options::_accountcode, ""));

    Config::StringSet activation_strings;
    activation_strings.insert("auto");
    activation_strings.insert("manual");

    Globals::options.add(Config::Option("kommuter-activation", &Options::_kommuter_activation , "auto", activation_strings));
    Globals::options.add(Config::Option("kommuter-timeout",    &Options::_kommuter_timeout ,(unsigned int) 10 , (unsigned int) 0 , (unsigned int) 255));

    Globals::options.add(Config::Option("audio-packet-length", &Options::_audio_packet_size,
         (unsigned int)KHOMP_READ_PACKET_SIZE, (unsigned int)KHOMP_MIN_READ_PACKET_SIZE, (unsigned int)KHOMP_MAX_READ_PACKET_SIZE, 8u));

    Globals::options.add(Config::Option("user-transfer-digits", &Options::_user_xfer_digits, ""));
    
    /* aliases */
    Globals::options.synonym("context-gsm", "context-gsm-call");
    Globals::options.synonym("context-gsm-alt", "context-gsm-call-alt");
    Globals::options.synonym("echocanceller", "echo-canceller");
    Globals::options.synonym("dtmfsuppression", "out-of-band-dtmfs");
    Globals::options.synonym("dtmfsupression", "out-of-band-dtmfs");
    Globals::options.synonym("pulsedetection", "pulse-forwarding");
    Globals::options.synonym("r2preconnectwait", "r2-preconnect-wait");
    Globals::options.synonym("bridge", "native-bridge");
    Globals::options.synonym("suppressiondelay", "suppression-delay");
    Globals::options.synonym("log", "log-to-disk");
    Globals::options.synonym("volume", "output-volume");
    Globals::options.synonym("disconnectdelay", "disconnect-delay");
}

void Opt::obtain(void)
{
    try
    {
        /* everything should start clean! */
        cleanConfiguration();        

        /* reset loaded options */
        Globals::options.reset(&Opt::_options);

        /* should be loaded *BEFORE* start_k3l */
        loadConfiguration("khomp.conf", NULL);

        /* commit, loading defaults where needed */
            Config::Options::Messages msgs = Globals::options.commit(&Opt::_options);

        /* config already full loaded at this point, so we can use our own log system... */
        for (Config::Options::Messages::iterator i = msgs.begin(); i != msgs.end(); i++)
        {
            DBG(FUNC,FMT("%s") % (*i).c_str());
        }
    }
    catch (std::runtime_error & e) 
    {    
        LOG(ERROR, FMT("unable to obtain general options: %s: procedure aborted!") % e.what());
    }    
}

void Opt::commit(void)
{
    processGroupString();

    /* Check FXS hotlines correcteness */
    OrigToDestMapType::const_iterator endOfHotlines = _fxs_hotline.end();

    for (OrigToDestMapType::const_iterator i = _fxs_hotline.begin(); i != endOfHotlines; i++)
    {  
        BranchToObjectMapType::const_iterator j = _fxs_branch_map.find(i->first);

        if (j == _fxs_branch_map.end())
        {   
            LOG(ERROR, FMT("unable to find branch '%s': hotline '%s' to '%s' is invalid!")
                % i->first % i->first % i->second);
        } 
    }   

    /* Check FXS options correcteness */
    BranchToOptMapType::const_iterator endOfOptions = _branch_options.end();
        
    for (BranchToOptMapType::const_iterator i = _branch_options.begin(); i != endOfOptions; i++)
    {   
        BranchToObjectMapType::const_iterator j = _fxs_branch_map.find(i->first);        
        if (j == _fxs_branch_map.end())
        {   
            LOG(ERROR, FMT("unable to find branch '%s' for options '%s'")
                % i->first % i->second);
        } 
    }   
}

void Opt::loadConfiguration(const char *file_name, const char **section, bool show_errors)
{
    switch_xml_t cfg, xml, settings;

    if (!(xml = switch_xml_open_cfg(file_name, &cfg, NULL)))
    {
        if (show_errors)
        {
            LOG(ERROR,FMT("Open of %s failed") % file_name);
        }

        return;
    }

    /* Load all the global settings pertinent to all boards */
    settings = processSimpleXML(cfg,"settings");
    
    /* Process channel settings */
    processSimpleXML(settings,"channels");

    /* Process groups settings */
    processGroupXML(settings);

    /* Process cadence settings */
    processCadenceXML(settings);

    /* Process fxs branches settings */
    processFXSBranchesXML(settings);

    /* Process hotlines settings */
    processFXSHotlines(settings);

    /* Process fxs options settings */
    processFXSOptions(settings);
    
    switch_xml_free(xml);
}

void Opt::cleanConfiguration(void)
{
    _fxs_orig_base.clear();
    _fxs_hotline.clear();
    _fxs_branch_map.clear();
    _branch_options.clear();

    _groups.clear();
    _cadences.clear();

    _cadences.insert(CadencesPairType("fast-busy", CadenceType(100,100)));
    _cadences.insert(CadencesPairType("ringback", CadenceType(1000,4000)));
    _cadences.insert(CadencesPairType("pbx-dialtone", CadenceType(1000,100)));
    _cadences.insert(CadencesPairType("co-dialtone", CadenceType(0,0)));
    _cadences.insert(CadencesPairType("vm-dialtone", CadenceType(1000,100,100,100)));
}

/*
void Options::AmaflagOption::operator()(const Config::StringType & str)
{
    //_value = Strings::tolong(str);
    //if(_value < 0)
    //    throw Config::Failure(STG(FMT("invalid AMA flags: %s") % str));
}

void Options::CallGroupOption::operator()(const Config::StringType & str)
{
    _groups = str;
}

void Options::PickupGroupOption::operator()(const Config::StringType & str)
{
    _groups = str;
}
*/

void Options::RecordPrefixOption::operator()(const Config::StringType & str)
{
    if (mkdir(str.c_str(), 493 /* 0755 */) < 0 && errno != EEXIST)
        throw Config::Failure("the default recording directory could not be created.");

    _value = str; 
}

void Options::CentralOfficeDialtone::operator()(const Config::StringType & str) 
{
    Strings::vector_type tokens;
    Strings::tokenize(str, tokens, ",");

    for (Strings::vector_type::iterator i = tokens.begin(); i != tokens.end(); i++) 
        _value.push_back(*i);
}

void Options::LogDiskOption::operator()(const Config::StringType & str) 
{
    K::Logger::processLogDisk(NULL, str, false, true);
}

void Options::LogConsoleOption::operator()(const Config::StringType & str) 
{
    K::Logger::processLogConsole(NULL, str, false, true);
}

void Options::LogTraceOption::operator()(const Config::StringType & str)
{
    Strings::vector_type tokens;
    Strings::tokenize(str, tokens, ",");

    bool         enable_k3l_tracing  = false;
    bool         enable_r2_tracing   = false;
    bool         enable_rdsi_tracing = false;

    for (Strings::vector_type::iterator i = tokens.begin(); i != tokens.end(); i++)
    {
        std::string tok = Strings::trim(*i);

        /**/ if (tok == "k3l")  enable_k3l_tracing = true;
        else if (tok == "r2")   enable_r2_tracing = true;
        else if (tok == "rdsi") enable_rdsi_tracing = true;
        else
        {
            LOG(ERROR, FMT("invalid string '%s' for option 'trace', ignoring...") % tok)
        }
    }

    Logfile logfile;

    /* k3l tracing */
    if (enable_k3l_tracing)
    {
        K::LogConfig::set(logfile, "K3L", "Value",        enable_k3l_tracing);
        K::LogConfig::set(logfile, "K3L", "CallProgress", enable_k3l_tracing);
        K::LogConfig::set(logfile, "K3L", "CallAnalyzer", enable_k3l_tracing);
        K::LogConfig::set(logfile, "K3L", "CadenceRecog", enable_k3l_tracing);
        K::LogConfig::set(logfile, "K3L", "CallControl",  enable_k3l_tracing);
        K::LogConfig::set(logfile, "K3L", "Fax",          enable_k3l_tracing);
    }

    /* r2 tracing */
    K::LogConfig::set(logfile, "R2", "Value",     enable_r2_tracing);
    K::LogConfig::set(logfile, "R2", "Signaling", enable_r2_tracing);
    K::LogConfig::set(logfile, "R2", "States",    enable_r2_tracing);

    /* ISDN tracing */
    if (enable_rdsi_tracing ^ Globals::flag_trace_rdsi)
    {
        K::LogConfig::set(logfile, "ISDN", "Value", enable_rdsi_tracing);
        K::LogConfig::set(logfile, "ISDN", "Lapd",  enable_rdsi_tracing);
        K::LogConfig::set(logfile, "ISDN", "Q931",  enable_rdsi_tracing);
    }

    try
    {
        Globals::k3lapi.command(-1, -1, CM_LOG_UPDATE);
    }
    catch(...)
    {
        LOG(ERROR,"Error while send command CM_LOG_UPDATE");
    }
}

switch_xml_t Opt::processSimpleXML(switch_xml_t &xml, const std::string& child_name)
{
    switch_xml_t param, child;

    if ((child = switch_xml_child(xml, child_name.c_str())))
    {
        for (param = switch_xml_child(child, "param"); param; param = param->next)
        {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            try
            {
                DBG(FUNC,FMT("loading '%s' options: '%s'...")
                    % var
                    % val);

                Globals::options.process(&Opt::_options, var, val);
            }
            catch (Config::Failure e)
            {
                LOG(ERROR,FMT("config processing error: %s. [%s=%s]")  
                    % e.what()
                    % var
                    % val);

            }
        }
    }

    return child;
}

void Opt::processGroupXML(switch_xml_t &xml)
{
    switch_xml_t param, child;

    if ((child = switch_xml_child(xml,"groups")))
    {
        for (param = switch_xml_child(child, "param"); param; param = param->next)
        {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            try
            {
                DBG(FUNC,FMT("loading group '%s' options: '%s'...")
                    % var
                    % val);

                _groups.insert(GroupToDestPairType(var,val));

            }
            catch (Config::Failure e)
            {
                LOG(ERROR,FMT("config processing error: %s. [%s=%s]")  
                    % e.what()
                    % var
                    % val);
            }
        }
    }
}

void Opt::processCadenceXML(switch_xml_t &xml)
{
    switch_xml_t param, child;

    if ((child = switch_xml_child(xml,"cadences")))
    {
        for (param = switch_xml_child(child, "param"); param; param = param->next)
        {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            try
            {
                DBG(FUNC,FMT("loading cadences '%s' options: '%s'...")
                    % var
                    % val);

                Strings::vector_type values;
                Strings::tokenize(val, values, " :,.");

                if (values.size() != 2 && values.size() != 4)
                {
                    LOG(WARNING, FMT("file '%s': wrong number of arguments at cadence '%s'!")
                        % "khomp.conf.xml"
                        % var);
                }
                else
                {
                    CadenceType cadence;

                    cadence.ring       = Strings::toulong(Strings::trim(values[0]));
                    cadence.ring_s     = Strings::toulong(Strings::trim(values[1]));

                    if (values.size() == 4)
                    {   
                        cadence.ring_ext   = Strings::toulong(Strings::trim(values[2]));
                        cadence.ring_ext_s = Strings::toulong(Strings::trim(values[3]));
                    }   

                    _cadences.erase(var); /* erases previous (possibly predefined) cadence */
                    _cadences.insert(CadencesPairType(var, cadence));
                } 
            }
            catch (Config::Failure e)
            {
                LOG(ERROR,FMT("config processing error: %s. [%s=%s]")  
                    % e.what()
                    % var
                    % val);

            }                  
            catch (Strings::invalid_value e)
            {   
               LOG(ERROR,FMT("file '%s': number expected at cadence '%s', got '%s'.") 
                    % "khomp.conf.xml"
                    % var
                    % e.value().c_str());
            }   
        }
    }
}

void Opt::processFXSBranchesXML(switch_xml_t &xml)
{
    switch_xml_t param, child;

    if ((child = switch_xml_child(xml,"fxs-branches")))
    {
        for (param = switch_xml_child(child, "param"); param; param = param->next)
        {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");


            DBG(FUNC,FMT("loading fxs-branches '%s' options: '%s'...")
                    % var
                    % val);

            try
            {
                unsigned long orig_number = Strings::toulong(var);

                /* gcc! stop complaining! */
                (void)orig_number;

                Strings::vector_type boards;
                Strings::tokenize(val, boards, " :,");

                if (boards.size() < 1)
                {
                    DBG(FUNC,FMT("file '%s': orig number '%s' without any board!")
                        % "khomp.conf.xml"
                        % var);
                    continue;
                }

                for (Strings::vector_type::iterator iter = boards.begin(); iter != boards.end(); iter++)
                {
                    /* quebrar em strings por vírgula/espaço */
                    unsigned long serial_number = Strings::toulong(Strings::trim(*iter));

                    bool found = false;

                    for (unsigned int device = 0; device < Globals::k3lapi.device_count(); device++)
                    {
                        const K3L_DEVICE_CONFIG & conf = Globals::k3lapi.device_config(device);

                        std::string str_serial(conf.SerialNumber);

                        if (Strings::toulong(str_serial) == serial_number)
                        {
                            found = true;

                            _fxs_orig_base.insert(BoardToOrigPairType(device, var));
                            break;
                        }
                    }

                    if (!found)
                    {
                        LOG(WARNING, FMT("file 'khomp.conf.xml': board with serial number '%u' not found!") 
                        % serial_number);

                        break;
                    }
                }
            }
            catch (Config::Failure e)
            {
                LOG(ERROR,FMT("config processing error: %s. [%s=%s]")  
                    % e.what()
                    % var
                    % val);
            }
            catch (Strings::invalid_value & e)
            {
                LOG(ERROR, D("invalid numeric value: %s") % e.value());
            }
            catch (...)
            {
                LOG(ERROR, D("config processing error..."));
            }
        }
    }
}

void Opt::processFXSHotlines(switch_xml_t &xml)
{
    switch_xml_t param, child;

    if ((child = switch_xml_child(xml,"fxs-hotlines")))
    {
        for (param = switch_xml_child(child, "param"); param; param = param->next)
        {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            try
            {
                DBG(FUNC,FMT("loading fxs-hotlines '%s' options: '%s'...")
                    % var
                    % val);

                unsigned long orig_number = Strings::toulong(var);

                (void)orig_number; /* stop complaining! */

                _fxs_hotline.insert(OrigToDestPairType(var, val));

            }
            catch (Config::Failure e)
            {
                LOG(ERROR,FMT("config processing error: %s. [%s=%s]")  
                    % e.what()
                    % var
                    % val);        
            }
            catch (Strings::invalid_value e)
            {
                LOG(WARNING, FMT("file '%s': number expected, got '%s'!") 
                    % "khomp.conf.xml" % e.value().c_str());
            }  
        }
    }
}

void Opt::processFXSOptions(switch_xml_t &xml)
{
    switch_xml_t param, child;

    if ((child = switch_xml_child(xml,"fxs-options")))
    {
        for (param = switch_xml_child(child, "param"); param; param = param->next)
        {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            try
            {
                DBG(FUNC,FMT("loading fxs-options '%s' options: '%s'...")
                    % var
                    % val);

                Strings::vector_type branches;
                Strings::tokenize(var, branches, " ,");

                if (branches.size() < 1)
                {   
                    //TODO: Get linenumber
                    LOG(WARNING, FMT("file '%s': no branches specified in line %d!") 
                    % "khomp.conf.xml" % 0);
                }           
                else
                {   
                    for (Strings::vector_type::iterator iter = branches.begin();
                            iter != branches.end(); iter++)
                    {                   
                        std::string tmp_branch = Strings::trim(*iter);

                        unsigned long branch_number = Strings::toulong(tmp_branch);
                        (void) branch_number;

                        _branch_options.insert(BranchToOptPairType(tmp_branch, val));
                    }
                }
            }
            catch (Config::Failure e)
            {
                LOG(ERROR,FMT("config processing error: %s. [%s=%s]")  
                    % e.what()
                    % var
                    % val);      
            }
            catch (Strings::invalid_value e)
            {
                LOG(WARNING, FMT("file '%s': number expected, got '%s'!") 
                    % "khomp.conf.xml" % e.value().c_str());
            }
        }
    }
}
