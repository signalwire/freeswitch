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
 * Description: This C++ header file describes the SqliteCDR class which handles formatting a CDR out to
 * a SQLite database using prepared statements.
 *
 * sqlitecdr.h
 *
 */

#include "baseregistry.h"
#include <list>
#include <sstream>

#ifndef SQLITECDR
#define SQLITECDR

class SqliteCDR : public BaseCDR {
	public:
		SqliteCDR();
		SqliteCDR(switch_mod_cdr_newchannel_t *newchannel);
		//SqliteCDR(const SqliteCDR& copyFrom);
		virtual ~SqliteCDR();
		virtual bool process_record();
		virtual void connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param);
		virtual void disconnect();
		virtual bool is_activated();
		virtual void tempdump_record();
		virtual void reread_tempdumped_records();
		virtual std::string get_display_name();

	private:
		static bool activated;
		static char sql_query[1024];
		static std::string tmp_sql_query; // Object must exist to bind the statement, this used for generating the sql
		static char sql_query_chanvars[100];
		static std::string db_filename;
		static bool use_utc_time;
		switch_time_t sqlite_callstartdate;
		switch_time_t sqlite_callanswerdate;
		switch_time_t sqlite_calltransferdate;
		switch_time_t sqlite_callenddate;
		static switch_core_db_t *db;
		static switch_core_db_stmt_t *stmt;
		static switch_core_db_stmt_t *stmt_chanvars;
		static switch_core_db_stmt_t *stmt_begin;
		static switch_core_db_stmt_t *stmt_commit;
		static bool connectionstate;
		static bool logchanvars;
		static std::list<std::string> chanvars_fixed_list;
		static std::vector<switch_mod_cdr_sql_types_t> chanvars_fixed_types;
		static std::list<std::string> chanvars_supp_list; // The supplemental list
		static bool repeat_fixed_in_supp;
		static std::string display_name;
};

#endif

/* For Emacs:
 * Local Variables:
 * mode:c++
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
