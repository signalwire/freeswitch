/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
*
* Copyright 2006, Author: Yossi Neiman, president of Cartis Solutions, Inc. 
* <freeswitch AT cartissolutions.com>
*
* Description: This C++ header file describes the MysqlCDR class which handles formatting a CDR out to
* a MySQL 4.1.x or greater server using prepared statements.
*
*
* Version: MPL 1.1
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License
*
* The Core API is the FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application by
* Anthony Minnesale II <anthmct@yahoo.com>
*
* mysqlcdr.h
*
*/

#include "baseregistry.h"
#include <list>
#include <sstream>
#include <mysql.h>

#ifndef MYSQLCDR
#define MYSQLCDR

class MysqlCDR : public BaseCDR {
	public:
		MysqlCDR();
		MysqlCDR(switch_mod_cdr_newchannel_t *newchannel);
		//MysqlCDR(const MysqlCDR& copyFrom);
		virtual ~MysqlCDR();
		virtual bool process_record();
		virtual void connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param);
		virtual void disconnect();
		virtual bool is_activated();
		virtual void tempdump_record();
		virtual void reread_tempdumped_records();

	private:
		static bool activated;
		static char *sql_query;
		static std::string tmp_sql_query; // Object must exist to bind the statement, this used for generating the sql
		static char sql_query_chanvars[100];
		static MYSQL *conn;
		static MYSQL_STMT *stmt;
		static MYSQL_STMT *stmt_chanvars;
		static bool connectionstate;
		static bool logchanvars;
		static std::list<std::string> chanvars_fixed_list;
		static std::vector<switch_mod_cdr_sql_types_t> chanvars_fixed_types;
		static std::list<std::string> chanvars_supp_list; // The supplemental list
		static bool repeat_fixed_in_supp;
		static char hostname[255];
		static char username[255];
		static char dbname[255];
		static char password[255];
		//static fstream tmpfile;
		std::vector<MYSQL_BIND> bindme;
		//MYSQL_BIND *bindme;
		MYSQL_TIME my_callstartdate;
		MYSQL_TIME my_callanswerdate;
		MYSQL_TIME my_callenddate;
		// Why all these long unsigned int's?  MySQL's prep statement API expects these to actually exist and not just be params passed to the function calls.  The are to measure the length of actual data in the char* arrays.
		long unsigned int clid_length;
		long unsigned int dialplan_length;
		long unsigned int myuuid_length;
		long unsigned int destuuid_length;
		long unsigned int src_length;
		long unsigned int dst_length;
		long unsigned int srcchannel_length;
		long unsigned int dstchannel_length;
		long unsigned int ani_length;
		long unsigned int ani2_length;
		long unsigned int lastapp_length;
		long unsigned int lastdata_length;
		// Now a couple internal methods
		template <typename T> void add_parameter(T& param, enum_field_types type, bool *is_null=0);
		void add_string_parameter(char* param, long unsigned int& param_length, enum_field_types type, bool* is_null=0);
		void set_mysql_time(switch_time_exp_t& param, MYSQL_TIME& destination);
};

#endif
