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
 * Description: This C++ header file describes the OdbcCDR class which handles formatting a CDR out to
 * an ODBC backend using prepared statements.
 *
 * odbccdr.h
 *
 */

#include "baseregistry.h"
#include <switch.h>
#include <iostream>
#include <list>
#include <sstream>

#ifndef __CYGWIN__
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#else
#include <windows.h>
#include <w32api/sql.h>
#include <w32api/sqlext.h>
#include <w32api/sqltypes.h>
#endif

#ifndef ODBCCDR
#define ODBCCDR

class OdbcCDR : public BaseCDR {
	public:
		OdbcCDR();
		OdbcCDR(switch_mod_cdr_newchannel_t *newchannel);
		//OdbcCDR(const MysqlCDR& copyFrom);
		virtual ~OdbcCDR();
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
		static modcdr_time_convert_t convert_time;
		static std::string display_name;
		static std::string tmp_sql_query; // Object must exist to bind the statement, this used for generating the sql
		static char sql_query_chanvars[355];
		static char sql_query_ping[10];
		static bool connectionstate;
		static bool logchanvars;
		static SQLHENV ODBC_env;     /* global ODBC Environment */
		static SQLHDBC ODBC_con;     /* global ODBC Connection Handle */
		static SQLHSTMT ODBC_stmt;
		static SQLHSTMT ODBC_stmt_chanvars;
		static SQLHSTMT ODBC_stmt_ping;
		static std::list<std::string> chanvars_fixed_list;
		static std::vector<switch_mod_cdr_sql_types_t> chanvars_fixed_types;
		static std::list<std::string> chanvars_supp_list; // The supplemental list
		static bool repeat_fixed_in_supp;
		static char dsn[255];
		static char hostname[255];
		static char username[255];
		static char dbname[255];
		static char password[255];
		static char tablename[255];
		static char tablename_chanvars[255];
		//static fstream tmpfile;
		char odbc_callstartdate[128];
		char odbc_callanswerdate[128];
		char odbc_calltransferdate[128];
		char odbc_callenddate[128];
		void disconnect_stage_1();
		void connect_to_database();
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
