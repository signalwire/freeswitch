/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 * Copyright 2006, Author: Yossi Neiman of Cartis Solutions, Inc. <freeswitch AT cartissolutions.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 *
 * The Initial Developer of the Original Code is
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 *
 * Description: This header describes the switch_mod_cdr_newchannel_t struct, the BaseCDR base class that all
 * CDR loggers will inherit from, and the switch_mod_cdr_sql_types_t enum for use with the SQL type loggers
 * (i.e. MySQL).  This is part of the mod_cdr module for Freeswitch by Anthony Minnesale and friends.
 *
 * basecdr.h
 *
 */

#ifndef BASECDR
#define BASECDR

#ifdef __cplusplus
#include <vector>
#include <list>
#include <vector>
#include <map>
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct switch_mod_cdr_newchannel_t 
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_channel_timetable_t *timetable;
	switch_caller_extension_t *callerextension;
	switch_caller_profile_t *callerprofile;
	switch_caller_profile_t *originateprofile;
	bool originate;
};

enum switch_mod_cdr_sql_types_t { CDR_INTEGER,CDR_STRING,CDR_DECIMAL,CDR_DOUBLE,CDR_TINY };

class BaseCDR {
	public:
		BaseCDR();
		virtual ~BaseCDR();
		BaseCDR(switch_mod_cdr_newchannel_t *newchannel);
		virtual void connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param) = 0;
		virtual void disconnect() = 0;
		virtual bool process_record() = 0;
		virtual bool is_activated() = 0;
		virtual void tempdump_record() = 0;
		virtual void reread_tempdumped_records() = 0;
	protected:
		void parse_channel_variables_xconfig(std::string& unparsed,std::list<std::string>& chanvarslist,bool fixed);
		void parse_channel_variables_xconfig(std::string& unparsed,std::list<std::string>& chanvarslist,std::vector<switch_mod_cdr_sql_types_t>& chanvars_fixed_types);  // Typically used for SQL types
		void process_channel_variables(const std::list<std::string>& stringlist,const std::list<std::string>& fixedlist,switch_channel_t *channel,bool repeat = 1); //This is used for supplemental chanvars
		void process_channel_variables(const std::list<std::string>& stringlist,switch_channel_t *channel); // This is used for fixed chanvars
		switch_time_t callstartdate;
		switch_time_t callanswerdate;
		switch_time_t callenddate;
		switch_call_cause_t hangupcause;
		char *hangupcause_text;
		char clid[80];
		bool originated;  // Did they originate this call?
		char dialplan[80];
		char myuuid[37]; // 36 + 1 to hold \0
		char destuuid[37];
		char src[80];
		char dst[80];
		char srcchannel[80];
		char dstchannel[80];
		char ani[80];
		char aniii[80];
		char network_addr[40];
		char lastapp[80];
		char lastdata[255]; 
		switch_time_t billusec; // Yes, you heard me, we're measuring in microseconds
		int disposition; // Currently 0 = Busy/Unanswered, 1 = Answered
		int amaflags;
		switch_core_session_t *coresession;
		std::list<std::pair<std::string,std::string> > chanvars_fixed;
		std::map<std::string,std::string> chanvars_supp;
		bool errorstate; // True if there is an error writing the log
};

#endif
#endif
