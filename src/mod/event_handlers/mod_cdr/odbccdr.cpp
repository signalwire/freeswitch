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
 * Description: This C++ source file describes the OdbcCDR class which handles formatting a CDR out to
 * an ODBC backend using prepared statements.
 *
 * odbccdr.cpp
 *
 */

#include "odbccdr.h"

OdbcCDR::OdbcCDR() : BaseCDR()
{

}

OdbcCDR::OdbcCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
{
	if(newchannel != 0)
	{
		switch_time_exp_t tempcallstart, tempcallanswer, tempcalltransfer, tempcallend;
		memset(&tempcallstart,0,sizeof(tempcallstart));
		memset(&tempcallanswer,0,sizeof(tempcallanswer));
		memset(&tempcalltransfer,0,sizeof(tempcalltransfer));
		memset(&tempcallend,0,sizeof(tempcallend));
		convert_time(&tempcallstart, callstartdate);
		convert_time(&tempcallanswer, callanswerdate);
		convert_time(&tempcalltransfer, calltransferdate);
		convert_time(&tempcallend, callenddate);
		
		// Format the times
		switch_size_t retsizecsd, retsizecad, retsizectd, retsizeced;  //csd == callstartdate, cad == callanswerdate, ced == callenddate, ceff == callenddate_forfile
		char format[] = "%Y-%m-%d %T";
		switch_strftime(odbc_callstartdate,&retsizecsd,sizeof(odbc_callstartdate),format,&tempcallstart);
		switch_strftime(odbc_callanswerdate,&retsizecad,sizeof(odbc_callanswerdate),format,&tempcallanswer);
		switch_strftime(odbc_calltransferdate,&retsizectd,sizeof(odbc_calltransferdate),format,&tempcalltransfer);
		switch_strftime(odbc_callenddate,&retsizeced,sizeof(odbc_callenddate),format,&tempcallend);
		
		if(chanvars_fixed_list.size() > 0)
			process_channel_variables(chanvars_fixed_list,newchannel->channel);
		
		if(chanvars_supp_list.size() > 0)
			process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,repeat_fixed_in_supp);
	}	
}

OdbcCDR::~OdbcCDR()
{

}

bool OdbcCDR::connectionstate = 0;
bool OdbcCDR::logchanvars = 0;
bool OdbcCDR::repeat_fixed_in_supp = 0;
std::string OdbCDR::display_name = "OdbcCDR - The Open Database Backend Connector CDR logger backend";
modcdr_time_convert_t OdbcCDR::convert_time;
std::list<std::string> OdbcCDR::chanvars_fixed_list;
std::list<std::string> OdbcCDR::chanvars_supp_list;
std::vector<switch_mod_cdr_sql_types_t> OdbcCDR::chanvars_fixed_types;
bool OdbcCDR::activated = 0;
char OdbcCDR::sql_query[1024] = "";
std::string OdbcCDR::tmp_sql_query;
char OdbcCDR::sql_query_chanvars[355] = "";
char OdbcCDR::sql_query_ping[10] = "";
SQLHENV OdbcCDR::ODBC_env=0;
SQLHDBC OdbcCDR::ODBC_con=0;
SQLHSTMT OdbcCDR::ODBC_stmt=0;
SQLHSTMT OdbcCDR::ODBC_stmt_chanvars = 0;
SQLHSTMT OdbcCDR::ODBC_stmt_ping = 0;
char OdbcCDR::dsn[255] = "";
char OdbcCDR::hostname[255] = "";
char OdbcCDR::username[255] ="";
char OdbcCDR::dbname[255] = "";
char OdbcCDR::password[255] = "";
char OdbcCDR::tablename[255] = "";
char OdbcCDR::tablename_chanvars[255] = "";
//fstream OdbcCDR::tmpfile;

void OdbcCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	if(activated)
		disconnect();
	
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.
	
	int count_config_params = 0;  // Need to make sure all params are set before we load
	if ((settings = switch_xml_child(cfg, "odbccdr"))) 
	{
		for (param = switch_xml_child(settings, "param"); param; param = param->next) 
		{
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			
			if (!strcmp(var, "dsn"))
			{
				strncpy(dsn,val,strlen(val));
				count_config_params+=4;
			}
			else if (!strcmp(var, "hostname"))
			{
				if(val != 0)
				{
					strncpy(hostname,val,strlen(val));
					count_config_params++;
				}
			}
			else if (!strcmp(var, "username")) 
			{
				if(val != 0)
				{
					strncpy(username,val,strlen(val));
					count_config_params++;
				}
			}
			else if (!strcmp(var,"password"))
			{
				if(val != 0)
				{
					strncpy(password,val,strlen(val));
					count_config_params++;
				}
			}
			else if(!strcmp(var,"dbname"))
			{
				if(val != 0)
				{
					strncpy(dbname,val,strlen(val));
					count_config_params++;
				}
			}
			else if(!strcmp(var,"chanvars_fixed"))
			{
				std::string unparsed;
				unparsed = val;
				if(unparsed.size() > 0)
				{
					parse_channel_variables_xconfig(unparsed,chanvars_fixed_list,chanvars_fixed_types);
					//logchanvars=1;
				}
			}
			else if(!strcmp(var,"chanvars_supp"))
			{
				if(val != 0)
				{
					std::string unparsed = val;
					bool fixed = 0;
					logchanvars = 1;
					parse_channel_variables_xconfig(unparsed,chanvars_supp_list,fixed);
				}	
			}
			else if(!strcmp(var,"chanvars_supp_repeat_fixed"))
			{
				if(val != 0)
				{
					std::string repeat = val;
					if(repeat == "Y" || repeat == "y" || repeat == "1")
						repeat_fixed_in_supp = 1;
				}
			}
			else if(!strcmp(var,"main_db_table"))
			{
				if(val != 0)
					strncpy(tablename,val,strlen(val));
			}
			else if(!strcmp(var,"supp_chanvars_db_table"))
			{
				if(val != 0)
					strncpy(tablename_chanvars,val,strlen(val));
			}
			else if(!strcmp(var,"timezone"))
			{
				if(!strcmp(val,"utc"))
					convert_time = switch_time_exp_gmt;
				else if(!strcmp(val,"local"))
					convert_time = switch_time_exp_lt;
				else
				{
					switch_console_printf(SWITCH_CHANNEL_LOG,"Invalid configuration parameter for timezone.  Possible values are utc and local.  You entered: %s\nDefaulting to local.\n",val);
					convert_time = switch_time_exp_lt;
				}
			}
			
			if(strlen(tablename) == 0)
				strncpy(tablename,"freeswitchcdr",13);
			
			if(strlen(tablename_chanvars) && logchanvars)
				strncpy(tablename_chanvars,"chanvars",8);
		}
		
		if (count_config_params==4)
			activated = 1;
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"You did not specify the minimum parameters for using this module.  You must specify a DSN,hostname, username, password, and database to use OdbcCDR.  You only supplied %d parameters.\n",count_config_params);
		
		if(activated)
		{
			tmp_sql_query = "INSERT INTO ";
			tmp_sql_query.append(tablename);
			tmp_sql_query.append(" (callstartdate,callanswerdate,calltransferdate,callenddate,originated,clid,src,dst,ani,aniii,dialplan,myuuid,destuuid,srcchannel,dstchannel,lastapp,lastdata,billusec,disposition,hangupcause,amaflags");
			
			int items_appended = 0;
			
			if(chanvars_fixed_list.size() > 0 )
			{
				std::list<std::string>::iterator iItr, iEnd;
				for(iItr = chanvars_fixed_list.begin(), iEnd = chanvars_fixed_list.end(); iItr != iEnd; iItr++)
				{
					if(iItr->size() > 0)
					{
						tmp_sql_query.append(",");
						tmp_sql_query.append(*iItr);
						items_appended++;
					}
				}
			}
			
			tmp_sql_query.append(") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?");
			
			if(chanvars_fixed_list.size() > 0 )
			{
				for(int i = 0; i < items_appended; i++)
					tmp_sql_query.append(",?");
			}
			
			tmp_sql_query.append(")");
	
			std::string tempsql_query_chanvars = "INSERT INTO ";
			tempsql_query_chanvars.append(tablename_chanvars);
			tempsql_query_chanvars.append("(callid,varname,varvalue) VALUES(?,?,?)");
			memset(sql_query_chanvars,0,355);
			strncpy(sql_query_chanvars,tempsql_query_chanvars.c_str(),tempsql_query_chanvars.size());

			strncpy(sql_query,tmp_sql_query.c_str(),tmp_sql_query.size());
			strncpy(sql_query_ping,"SELECT 1;",9);
			connect_to_database();
		}
	}
}

void OdbcCDR::connect_to_database()
{
	if(connectionstate)
		disconnect_stage_1();
	
	int ODBC_res;
	
	if (ODBC_env == SQL_NULL_HANDLE || connectionstate == 0) 
	{
		ODBC_res = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &ODBC_env);
		if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO)) 
		{
			switch_console_printf(SWITCH_CHANNEL_LOG,"Error allocating a new ODBC handle.\n");
			connectionstate = 0;
		}
	}
	
	ODBC_res = SQLSetEnvAttr(ODBC_env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
	
	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO)) 
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Error with ODBCSetEnv\n");
		SQLFreeHandle(SQL_HANDLE_ENV, ODBC_env);
		connectionstate = 0;
	}
	
	ODBC_res = SQLAllocHandle(SQL_HANDLE_DBC, ODBC_env, &ODBC_con);

	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Error AllocHDB %d\n",ODBC_res);
		SQLFreeHandle(SQL_HANDLE_ENV, ODBC_env);
		connectionstate = 0;
	}
	
	SQLSetConnectAttr(ODBC_con, SQL_LOGIN_TIMEOUT, (SQLPOINTER *)10, 0);

	 /* Note that the username and password could be NULL here, but that is allowed in ODBC.
	    In this case, the default username and password will be used from odbc.conf */
	
	ODBC_res = SQLConnect(ODBC_con, (SQLCHAR*)dsn, SQL_NTS, (SQLCHAR*)username, SQL_NTS, (SQLCHAR*)password, SQL_NTS);
	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO)) 
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Error connecting to the ODBC database on %d\n",ODBC_res);
		SQLFreeHandle(SQL_HANDLE_DBC, ODBC_con);
		SQLFreeHandle(SQL_HANDLE_ENV, ODBC_env);
		connectionstate = 0;
	}
	else 
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Connected to %s\n", dsn);
		connectionstate = 1;
	}
	
	// Turn off autocommit and have it preserve the cursors even after commit
	SQLSetConnectAttr(ODBC_con, SQL_AUTOCOMMIT_OFF, NULL, 0);
	SQLSetConnectAttr(ODBC_con, SQL_CB_PRESERVE, NULL, 0);
	
	ODBC_res = SQLAllocHandle(SQL_HANDLE_STMT, ODBC_con, &ODBC_stmt);

	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Failure in allocating a prepared statement %d\n", ODBC_res);
		SQLFreeHandle(SQL_HANDLE_STMT, ODBC_stmt);
	}
		
	ODBC_res = SQLPrepare(ODBC_stmt, (unsigned char *)sql_query, SQL_NTS);

	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Error in preparing a statement: %d\n", ODBC_res);
		SQLFreeHandle(SQL_HANDLE_STMT, ODBC_stmt);
	}
		
	if(logchanvars)
	{
		ODBC_res = SQLAllocHandle(SQL_HANDLE_STMT, ODBC_con, &ODBC_stmt_chanvars);

		if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
		{
			switch_console_printf(SWITCH_CHANNEL_LOG,"Failure in allocating a prepared statement %d\n", ODBC_res);
			SQLFreeHandle(SQL_HANDLE_STMT, ODBC_stmt_chanvars);
		}
		
		ODBC_res = SQLPrepare(ODBC_stmt_chanvars, (unsigned char *)sql_query_chanvars, SQL_NTS);

		if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
		{
			switch_console_printf(SWITCH_CHANNEL_LOG,"Error in preparing a statement: %d\n", ODBC_res);
			SQLFreeHandle(SQL_HANDLE_STMT, ODBC_stmt);
		}
	}
	
	ODBC_res = SQLAllocHandle(SQL_HANDLE_STMT, ODBC_con, &ODBC_stmt_ping);
	
	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Failure in allocating a prepared statement %d\n", ODBC_res);
		SQLFreeHandle(SQL_HANDLE_STMT, ODBC_stmt_ping);
	}
	
	ODBC_res = SQLPrepare(ODBC_stmt_ping, (unsigned char *)sql_query_ping, SQL_NTS);
	
	if ((ODBC_res != SQL_SUCCESS) && (ODBC_res != SQL_SUCCESS_WITH_INFO))
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Error in preparing a statement: %d\n", ODBC_res);
		SQLFreeHandle(SQL_HANDLE_STMT, ODBC_stmt_ping);
	}
	
	if (ODBC_res == SQL_SUCCESS || ODBC_res == SQL_SUCCESS_WITH_INFO)
		connectionstate = 1;
}

bool OdbcCDR::is_activated()
{
	return activated;
}

void OdbcCDR::tempdump_record()
{

}

void OdbcCDR::reread_tempdumped_records()
{

}

bool OdbcCDR::process_record()
{
	for(int count=0, ODBC_res=-1; (ODBC_res != SQL_SUCCESS || ODBC_res != SQL_SUCCESS_WITH_INFO) && count < 5; count++)
	{
		int ODBC_res = SQLExecute(ODBC_stmt_ping);
		SQLFreeStmt(ODBC_stmt_ping,SQL_CLOSE);
		if(ODBC_res != SQL_SUCCESS && ODBC_res != SQL_SUCCESS_WITH_INFO)
		{
			// Try to reconnect and reprepare
			switch_console_printf(SWITCH_CHANNEL_LOG,"Error pinging the ODBC backend.  Attempt #%d to reconnect.\n",count+1);
			connect_to_database();
		}
	}
	
	int index = 1;
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(odbc_callstartdate), 0, odbc_callstartdate, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(odbc_callanswerdate), 0, odbc_callanswerdate, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(odbc_calltransferdate), 0, odbc_calltransferdate, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(odbc_callenddate), 0, odbc_callenddate, 0, 0);
	
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_TINYINT,0, 0,&originated, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(clid), 0, clid, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(src), 0, src, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(dst), 0, dst, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(ani), 0, ani, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(aniii), 0, aniii, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(dialplan), 0, dialplan, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(myuuid), 0, myuuid, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(destuuid), 0, destuuid, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(srcchannel), 0, srcchannel, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(dstchannel), 0, dstchannel, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(lastapp), 0, lastapp, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(lastdata), 0, lastdata, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &billusec, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_TINYINT, 0, 0, &disposition, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &hangupcause, 0, 0);
	SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_TINYINT, 0, 0, &amaflags, 0, 0);
		
	std::list<void*> temp_chanvars_holder; // This is used for any fixed chanvars, as we don't want things out of scope
	
	if(chanvars_fixed_list.size() > 0)
	{
		switch_size_t i = 0; // temporary variable, i is current spot on the string of types
		std::list<std::pair<std::string,std::string> >::iterator iItr, iEnd;
		for(iItr = chanvars_fixed.begin(), iEnd = chanvars_fixed.end(); iItr != iEnd; iItr++)
		{	
			switch(chanvars_fixed_types[i])
			{
				case CDR_INTEGER: 
				{
					int* x = new int;
					*x = 0;
					
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> *x;
					}
					
					temp_chanvars_holder.push_back(x);
					SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_CHAR, 0, 0, x, 0, 0);
					break;
				}
				case CDR_DOUBLE: 
				{
					double* x = new double;
					*x = 0;
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> *x;
					}
					
					temp_chanvars_holder.push_back(x);
					SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0, x, 0, 0);
					break;
				}
				case CDR_TINY:
				{
					short* x = new short;
					*x = 0;
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> *x;
					}
					
					temp_chanvars_holder.push_back(x);
					SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_TINYINT, 0, 0, x, 0, 0);
					break;
				}
				case CDR_STRING:
				{
					uint64_t stringlength = iItr->second.size();
					char* x = new char[(stringlength+1)];
					strncpy(x,iItr->second.c_str(),stringlength);
					temp_chanvars_holder.push_back(x);
					SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, x, 0, 0);
					break;
				}
				case CDR_DECIMAL:
				{
					uint64_t stringlength = iItr->second.size();
					char* x = new char[(stringlength+1)];
					strncpy(x,iItr->second.c_str(),stringlength);
					SQLBindParameter(ODBC_stmt, index++, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_DECIMAL, 0, 0, x, 0, 0);
					temp_chanvars_holder.push_back(x);
					break;
				}
				default:
					switch_console_printf(SWITCH_CHANNEL_LOG,"We should not get to this point in this switch/case statement.\n");
			}
			i++;
		}
	}
	
	SQLExecute(ODBC_stmt);
	
	if(logchanvars && chanvars_supp.size() > 0 && errorstate == 0)
	{
		/* Since autoincrement is a bane of the SQL rdbms industry, we have to use the myuuid
		   instead to link the tables.  Unfortunately, this is very wasteful of space, and not
		   highly recommended to use on heavily loaded systems.
		*/
		
		std::map<std::string,std::string>::iterator iItr,iBeg,iEnd;
		iEnd = chanvars_supp.end();
		for(iItr = chanvars_supp.begin(); iItr != iEnd; iItr++)
		{
			std::vector<char> tempfirstvector(iItr->first.begin(), iItr->first.end());
			tempfirstvector.push_back('\0');
			char* varname_temp = &tempfirstvector[0];
			
			std::vector<char> tempsecondvector(iItr->second.begin(), iItr->second.end());
			tempsecondvector.push_back('\0');
			char* varvalue_temp = &tempsecondvector[0];
			
			SQLBindParameter(ODBC_stmt_chanvars, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, sizeof(myuuid), 0, myuuid, 0, 0);
			
			SQLBindParameter(ODBC_stmt_chanvars, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, iItr->first.size(), 0, varname_temp, 0, 0);
			
			SQLBindParameter(ODBC_stmt_chanvars, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, iItr->second.size(), 0, varvalue_temp, 0, 0);
			
			int ODBC_res_chanvars = SQLExecute(ODBC_stmt_chanvars);
			if(ODBC_res_chanvars != SQL_SUCCESS && ODBC_res_chanvars != SQL_SUCCESS_WITH_INFO)
				errorstate = 0;
			else
				errorstate = 1;
		}
	}
			
	if(errorstate)
		SQLEndTran(SQL_HANDLE_DBC,ODBC_con,SQL_ROLLBACK);
	else
		SQLEndTran(SQL_HANDLE_DBC,ODBC_con,SQL_COMMIT);
	
	if(temp_chanvars_holder.size() > 0)
	{
		std::string::size_type i = 0, j = chanvars_fixed_types.size();
		for(; i < j ; i++)
		{
			switch(chanvars_fixed_types[i])
			{
				case CDR_STRING:
				case CDR_DECIMAL:
				{
					char* tempstring = (char*) temp_chanvars_holder.front();
					temp_chanvars_holder.pop_front();
					delete [] tempstring;
					break;
				}
				case CDR_INTEGER:
				{
					int* tempint = (int*) temp_chanvars_holder.front();
					temp_chanvars_holder.pop_front();
					delete tempint;
					break;
				}
				case CDR_DOUBLE:
				{
					double* tempdouble = (double*) temp_chanvars_holder.front();
					temp_chanvars_holder.pop_front();
					delete tempdouble;
					break;
				}
				case CDR_TINY:
				{	
					short* tempshort = (short*) temp_chanvars_holder.front();
					temp_chanvars_holder.pop_front();
					delete tempshort;
					break;
				}
				default:
					switch_console_printf(SWITCH_CHANNEL_LOG,"We should not get to this point in this switch/case statement.\n");
			}
		}
	}
	return 1;
}

void OdbcCDR::disconnect()
{
	disconnect_stage_1();
	
	activated = 0;
	logchanvars = 0;
	chanvars_fixed_list.clear();
	chanvars_supp_list.clear();
	chanvars_fixed_types.clear();
	connectionstate = 0;
	memset(hostname,0,255);
	memset(username,0,255);
	memset(password,0,255);
	memset(dbname,0,255);
	memset(sql_query,0,1024);
	tmp_sql_query.clear();
	//tmp_sql_query_chanvars.clear();
}

void OdbcCDR::disconnect_stage_1()
{
	SQLFreeStmt(ODBC_stmt,SQL_UNBIND);
	if(logchanvars)
		SQLFreeStmt(ODBC_stmt_chanvars,SQL_UNBIND);
	
	SQLDisconnect(ODBC_con);
	SQLFreeHandle(SQL_HANDLE_DBC, ODBC_con);
	SQLFreeHandle(SQL_HANDLE_ENV, ODBC_env);
	connectionstate = 0;
}

std::string OdbcCDR::get_display_name()
{
	return display_name;
}

AUTO_REGISTER_BASECDR(OdbcCDR);

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
