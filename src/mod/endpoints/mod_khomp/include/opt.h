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

#ifndef _OPT_H_
#define _OPT_H_

#include <string>
#include <map>
#include <vector>

#include <config_options.hpp>

#include "switch.h"
#include "utils.h"

typedef std::map    < std::string, std::string >       OrigToDestMapType;
typedef std::pair   < std::string, std::string >      OrigToDestPairType;
typedef std::vector < std::string >                       DestVectorType;

typedef std::map    < unsigned int, std::string >     BoardToOrigMapType;
typedef std::pair   < unsigned int, std::string >    BoardToOrigPairType;

typedef std::map    < std::string, unsigned int >      OrigToNseqMapType;
typedef std::pair   < std::string, unsigned int >     OrigToNseqPairType;

typedef std::pair   < unsigned int,unsigned int >           ObjectIdType;
typedef std::pair   < std::string, ObjectIdType > BranchToObjectPairType;
typedef std::map    < std::string, ObjectIdType >  BranchToObjectMapType;

typedef std::map    < std::string, std::string >      BranchToOptMapType;
typedef std::pair   < std::string, std::string >     BranchToOptPairType;

typedef std::map    < std::string, std::string >      GroupToDestMapType;
typedef std::pair   < std::string, std::string >     GroupToDestPairType;

struct CadenceType
{
    CadenceType(void)
    : ring(0), ring_s(0), ring_ext(0), ring_ext_s(0) {};

    CadenceType(unsigned int _ring, unsigned int _ring_s)
    : ring(_ring), ring_s(_ring_s), ring_ext(0), ring_ext_s(0) {};

    CadenceType(unsigned int _ring, unsigned int _ring_s, unsigned int _ring_ext, unsigned int _ring_ext_s)
    : ring(_ring), ring_s(_ring_s), ring_ext(_ring_ext), ring_ext_s(_ring_ext_s) {};

    unsigned int ring;
    unsigned int ring_s;
    unsigned int ring_ext;
    unsigned int ring_ext_s;
};

typedef std::pair < std::string, CadenceType > CadencesPairType;
typedef std::map  < std::string, CadenceType >  CadencesMapType;

struct Options
{
    Config::Value< bool >        _debug;
    Config::Value< std::string > _dialplan;
    Config::Value< std::string > _context;
    
    Config::Value< bool > _echo_canceller;
    Config::Value< bool > _auto_gain_control;
    Config::Value< bool > _out_of_band_dtmfs;
    Config::Value< bool > _suppression_delay;
    Config::Value< bool > _pulse_forwarding;
    Config::Value< bool > _native_bridge;
    Config::Value< bool > _recording;
    Config::Value< bool > _has_ctbus;
    Config::Value< bool > _fxs_bina;
    Config::Value< bool > _fxs_sharp_dial;
    Config::Value< bool > _drop_collect_call;
    Config::Value< bool > _ignore_letter_dtmfs;
    Config::Value< bool > _optimize_audio_path;
    
    Config::Value< bool > _fxo_send_pre_audio;
    Config::Value< unsigned int > _fxo_busy_disconnection;

    Config::Value< bool         > _auto_fax_adjustment;
    Config::Value< unsigned int > _fax_adjustment_timeout;

    Config::Value< bool         > _r2_strict_behaviour;
    Config::Value< unsigned int > _r2_preconnect_wait;

    Config::Value< unsigned int > _fxs_digit_timeout;
    Config::Value< unsigned int > _transferdigittimeout;

    Config::Value< std::string > _flash;
    Config::Value< std::string > _blindxfer;
    Config::Value< std::string > _atxfer;

    Config::Value< unsigned int > _ringback_co_delay;
    Config::Value< unsigned int > _ringback_pbx_delay;

    Config::Value< unsigned int > _disconnect_delay;

    Config::Value< int > _input_volume;
    Config::Value< int > _output_volume;

    struct CentralOfficeDialtone : public Config::FunctionValue
    {   
        void operator ()(const Config::StringType &); 
        const DestVectorType & operator()(void) const { return _value; };
        void clear(void)
        {   
            _value.clear();
        }   

    protected:
        DestVectorType _value;
    } _fxs_co_dialtone;

    struct LogDiskOption : public Config::FunctionValue
    {
        void operator ()(const Config::StringType &);
    } _log_disk_option;

    /*
    struct CallGroupOption: public Config::FunctionValue
    {   
        void operator ()(const Config::StringType &); 
        const std::string operator()(void) const { return _groups; };
    protected:
        std::string _groups;
    } _callgroup;

    struct PickupGroupOption: public Config::FunctionValue
    { 
        void operator ()(const Config::StringType &); 
        const std::string operator()(void) const { return _groups; };
    protected:
        std::string _groups;
    } _pickupgroup; // or intercept 
    */

    struct LogConsoleOption : public Config::FunctionValue
    {
        void operator ()(const Config::StringType &);
    } _log_console_option;

    struct LogTraceOption : public Config::FunctionValue
    {
        void operator ()(const Config::StringType &);
    } _log_trace_option;

    struct RecordPrefixOption: public Config::FunctionValue
    {   
        void operator ()(const Config::StringType &); 
        const std::string & operator()(void) const { return _value; };

    protected:
        std::string _value;
    } _record_prefix;

    Config::Value< std::string > _user_xfer_digits;
    Config::Value< std::string > _fxs_global_orig_base;

    Config::Value< std::string > _global_mohclass;
    Config::Value< std::string > _global_language;

    Config::Value< std::string > _context_gsm_call;
    Config::Value< std::string > _context2_gsm_call;
    Config::Value< std::string > _context_gsm_sms;
    Config::Value< std::string > _context_fxo;
    Config::Value< std::string > _context2_fxo;
    Config::Value< std::string > _context_fxs;
    Config::Value< std::string > _context2_fxs;
    Config::Value< std::string > _context_digital;
    Config::Value< std::string > _context_pr;

    Config::Value< std::string > _callgroup;
    Config::Value< std::string > _pickupgroup; /* or intercept */
    Config::Value< std::string > _accountcode;
    
    Config::Value< unsigned int > _kommuter_timeout;
    Config::Value< std::string  > _kommuter_activation;

    Config::Value< unsigned int > _audio_packet_size;
};

struct Opt
{
    /* here we load [cadences] */
    static CadencesMapType        _cadences;

    /* here we load [groups] */
    static GroupToDestMapType     _groups;

    /* here we load [fxs-hotlines] */
    static OrigToDestMapType      _fxs_hotline;

    /* here we load [fxs-branches] */
    static BoardToOrigMapType     _fxs_orig_base;

    /* here we load [fxs-options] */
    static BranchToOptMapType     _branch_options;

    /* here we load ... hannnn */
    static BranchToObjectMapType  _fxs_branch_map;

    static Options _options;

    /* Member functions */
    static void initialize(void);
    static void obtain(void);
    static void commit(void);

    //TODO: reload options at reloadxml ?
    static void reload(void); 

    protected:
    static void loadConfiguration(const char *, const char **, bool show_errors = true);
    static void cleanConfiguration(void);

    static switch_xml_t processSimpleXML(switch_xml_t &xml, const std::string& child_name);
    static void processGroupXML(switch_xml_t &xml);
    static void processCadenceXML(switch_xml_t &xml);
    static void processFXSBranchesXML(switch_xml_t &xml);
    static void processFXSHotlines(switch_xml_t &xml);
    static void processFXSOptions(switch_xml_t &xml);
};



#endif /* _OPT_H_ */

