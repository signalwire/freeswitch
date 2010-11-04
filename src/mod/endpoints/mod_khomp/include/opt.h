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
#include "switch.h"
#include "utils.h"

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

struct CSpan {
    std::string _dialplan;
    std::string _context;
    std::string _dialstring;
};

struct Opt
{
    typedef std::pair   < std::string, CadenceType >  CadencesPairType;
    typedef std::map    < std::string, CadenceType >  CadencesMapType;

    typedef std::map    < std::string, std::string >  OrigToDestMapType;
    typedef std::pair   < std::string, std::string >  OrigToDestPairType;
    typedef std::vector < std::string >               DestVectorType;

    typedef std::map    < unsigned int, std::string > BoardToOrigMapType;
    typedef std::pair   < unsigned int, std::string > BoardToOrigPairType;

    typedef std::map    < std::string, unsigned int > OrigToNseqMapType;
    typedef std::pair   < std::string, unsigned int > OrigToNseqPairType;

    typedef std::pair   < unsigned int,unsigned int > ObjectIdType;
    typedef std::pair   < std::string, ObjectIdType > BranchToObjectPairType;
    typedef std::map    < std::string, ObjectIdType > BranchToObjectMapType;

    typedef std::map    < std::string, std::string >  BranchToOptMapType;
    typedef std::pair   < std::string, std::string >  BranchToOptPairType;
    
    typedef std::map    < std::string, std::string >  GroupToDestMapType;
    typedef std::pair   < std::string, std::string >  GroupToDestPairType;

    typedef std::pair   < std::string, CSpan >        SpanPairType;

    typedef enum
    {
            GFLAG_MY_CODEC_PREFS = (1 << 0)
    }
    GFLAGS;

    static void initialize(void);
    static void obtain(void);
    static void commit(void);
    static void printConfiguration(switch_stream_handle_t*);

protected:

    static void loadConfiguration(const char *, const char **, bool show_errors = true);
    static void cleanConfiguration(void);
    
    static switch_xml_t processSimpleXML(switch_xml_t &xml, const std::string& child_name);
    static void processGroupXML(switch_xml_t &xml);
    static void processCadenceXML(switch_xml_t &xml);
    static void processFXSBranchesXML(switch_xml_t &xml);
    static void processFXSHotlines(switch_xml_t &xml);
    static void processFXSOptions(switch_xml_t &xml);

public:
    static bool                            _debug;
    static std::string                     _dialplan;
    static std::string                     _context;
    static std::map < std::string, CSpan > _spans;
	static GroupToDestMapType              _groups;
	static CadencesMapType                 _cadences;
    
    static bool  _echo_canceller;
    static bool  _auto_gain_control;
    static bool  _out_of_band_dtmfs;
    static bool  _suppression_delay;
    static bool  _pulse_forwarding;
    static bool  _native_bridge;
    static bool  _recording;
    static bool  _has_ctbus;
    static bool  _fxs_bina;
    static bool  _fxo_send_pre_audio;
    static bool  _drop_collect_call;
    static bool  _ignore_letter_dtmfs;
    static bool  _optimize_audio_path;

    static bool         _auto_fax_adjustment;
    static unsigned int _fax_adjustment_timeout;

    static bool         _r2_strict_behaviour;
    static unsigned int _r2_preconnect_wait;

    static unsigned int _fxs_digit_timeout;

    static unsigned int _transferdigittimeout;

    static std::string _flash;
    static std::string _blindxfer;
    static std::string _atxfer;

    static unsigned int _ringback_co_delay;
    static unsigned int _ringback_pbx_delay;

    static unsigned int _disconnect_delay;

    static int _input_volume;
    static int _output_volume;

    static DestVectorType    _fxs_co_dialtone;
    static OrigToDestMapType _fxs_hotline;
    static std::string       _fxs_global_orig_base;

    static BoardToOrigMapType    _fxs_orig_base;
    static BranchToObjectMapType _fxs_branch_map;
    static BranchToOptMapType    _branch_options;

    static std::string _global_mohclass;
    static std::string _global_language;

    static std::string _record_prefix;

    static std::string  _context_gsm_call;
    static std::string  _context2_gsm_call;
    static std::string  _context_gsm_sms;
    static std::string  _context_fxo;
    static std::string  _context2_fxo;
    static std::string  _context_fxs;
    static std::string  _context2_fxs;
    static std::string  _context_digital;
    static std::string  _context_pr;
    static std::string  _user_xfer;

    static int                _amaflags;
    static std::string _callgroup;
    static std::string _pickupgroup; /* or intercept */

    static std::string _accountcode;
    
    static unsigned int _kommuter_timeout;
    static std::string  _kommuter_activation;

    static unsigned int _audio_packet_size;

protected:

    struct ProcessFXSCODialtone
    {
        void operator()(std::string options);
    };

    struct ProcessRecordPrefix
    {
        void operator()(std::string path);
    };

    struct ProcessAMAFlags
    {
        void operator()(std::string options);
    };

    struct ProcessCallGroup
    {
        void operator()(std::string options);
    };

    struct ProcessPickupGroup
    {
        void operator()(std::string options);
    };

    struct ProcessLogOptions
    {
        ProcessLogOptions(output_type output): _output(output) {}; 

        void operator()(std::string options); 

        protected:
            output_type _output;
    };

    struct ProcessTraceOptions
    {
        void operator()(std::string options);
    };
};



#endif /* _OPT_H_ */

