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
 * Description: This C++ source file describes the MysqlCDR class which handles formatting a CDR out to
 * a MySQL 4.1.x or greater server using prepared statements.
 *
 * mysqlcdr.cpp
 *
 */

#ifdef WIN32
#include <Winsock2.h>
#endif
#include <mysql.h>
#include <switch.h>
#include <cstring>
#include <iostream>
#include "mysqlcdr.h"

MysqlCDR::MysqlCDR() : BaseCDR()
{

}

MysqlCDR::MysqlCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
{
	if(newchannel != 0)
	{
		clid_length = (long unsigned int)strlen(clid);
		src_length = (long unsigned int)strlen(src);
		dst_length = (long unsigned int)strlen(dst);
		ani_length = (long unsigned int)strlen(ani);
		aniii_length = (long unsigned int)strlen(aniii);
		dialplan_length = (long unsigned int)strlen(dialplan);
		myuuid_length = (long unsigned int)strlen(myuuid);
		destuuid_length = (long unsigned int)strlen(destuuid);
		srcchannel_length = (long unsigned int)strlen(srcchannel);
		dstchannel_length = (long unsigned int)strlen(dstchannel);
		lastapp_length = (long unsigned int)strlen(lastapp);
		lastdata_length = (long unsigned int)strlen(lastdata);
		network_addr_length = (long unsigned int)strlen(network_addr);
		
		if(chanvars_fixed_list.size() > 0)
			process_channel_variables(chanvars_fixed_list,newchannel->channel);
		
		if(chanvars_supp_list.size() > 0)
			process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,repeat_fixed_in_supp);
	}	
}

MysqlCDR::~MysqlCDR()
{

}

bool MysqlCDR::connectionstate = 0;
bool MysqlCDR::logchanvars = 0;
bool MysqlCDR::repeat_fixed_in_supp = 0;
std::list<std::string> MysqlCDR::chanvars_fixed_list;
std::list<std::string> MysqlCDR::chanvars_supp_list;
std::vector<switch_mod_cdr_sql_types_t> MysqlCDR::chanvars_fixed_types;
bool MysqlCDR::activated = 0;
char MysqlCDR::sql_query[1024] = "";
std::string MysqlCDR::tmp_sql_query;
char MysqlCDR::sql_query_chanvars[100] = "";
MYSQL* MysqlCDR::conn = 0;
MYSQL_STMT* MysqlCDR::stmt=0;
MYSQL_STMT* MysqlCDR::stmt_chanvars=0;
char MysqlCDR::hostname[255] = "";
char MysqlCDR::username[255] ="";
char MysqlCDR::dbname[255] = "";
char MysqlCDR::password[255] = "";
modcdr_time_convert_t MysqlCDR::convert_time = switch_time_exp_lt;
std::string MysqlCDR::display_name = "MysqlCDR - The MySQL 4.1+ CDR logger using prepared statements";
//fstream MysqlCDR::tmpfile;

void MysqlCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	if(activated)
		disconnect();
	
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.
	
	int count_config_params = 0;  // Need to make sure all params are set before we load
	if ((settings = switch_xml_child(cfg, "mysqlcdr"))) 
	{
		for (param = switch_xml_child(settings, "param"); param; param = param->next) 
		{
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "hostname"))
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
		}
		
		if (count_config_params==4)
			activated = 1;
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"You did not specify the minimum parameters for using this module.  You must specify a hostname, username, password, and database to use MysqlCDR.  You only supplied %d parameters.\n",count_config_params);
		
		if(activated)
		{
			tmp_sql_query = "INSERT INTO freeswitchcdr  (callstartdate,callanswerdate,calltransferdate,callenddate,originated,clid,src,dst,ani,aniii,dialplan,myuuid,destuuid,srcchannel,dstchannel,network_addr,lastapp,lastdata,billusec,disposition,hangupcause,amaflags";
			
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
			
			tmp_sql_query.append(") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?");
			
			if(chanvars_fixed_list.size() > 0 )
			{
				for(int i = 0; i < items_appended; i++)
					tmp_sql_query.append(",?");
			}
			
			tmp_sql_query.append(")");
	
			char tempsql_query_chanvars[] = "INSERT INTO chanvars (callid,varname,varvalue) VALUES(?,?,?)";
			memset(sql_query_chanvars,0,100);
			strncpy(sql_query_chanvars,tempsql_query_chanvars,strlen(tempsql_query_chanvars));

			strncpy(sql_query,tmp_sql_query.c_str(),tmp_sql_query.size());
			connect_to_database();
		}
	}
}

void MysqlCDR::connect_to_database()
{
	conn = mysql_init(NULL);
	mysql_options(conn, MYSQL_READ_DEFAULT_FILE, "");
	if(mysql_real_connect(conn,hostname,username,password,dbname,0,NULL,0) == NULL)
	{
		const char *error1 = mysql_error(conn);
		switch_console_printf(SWITCH_CHANNEL_LOG,"Cannot connect to MySQL Server.  The error was: %s\n",error1);
	}
	else
		connectionstate = 1;
	
	mysql_autocommit(conn,0);
	stmt = mysql_stmt_init(conn);
		
	mysql_stmt_prepare(stmt,sql_query,(long unsigned int)strlen(sql_query));
		
	if(logchanvars)
	{
		stmt_chanvars = mysql_stmt_init(conn);
		mysql_stmt_prepare(stmt_chanvars,sql_query_chanvars,(long unsigned int)strlen(sql_query_chanvars));
	}
}

bool MysqlCDR::is_activated()
{
	return activated;
}

template <typename T> 
void MysqlCDR::add_parameter(T& param, enum_field_types type, bool *is_null)
{
	MYSQL_BIND temp_bind;
	memset(&temp_bind,0,sizeof(temp_bind));
	
        temp_bind.buffer_type = type;
	if(is_null != 0)
	{
	        if(*is_null)
	                temp_bind.is_null = (my_bool*) is_null;
		else
			temp_bind.buffer = &param;
	}
	else
		temp_bind.buffer = &param;

	bindme.push_back(temp_bind);
}

template <>
void MysqlCDR::add_parameter<MYSQL_TIME>(MYSQL_TIME& param, enum_field_types type, bool* is_null)
{
	MYSQL_BIND temp_bind;
	memset(&temp_bind,0,sizeof(temp_bind));
	
	temp_bind.buffer_type = type;
	if(is_null != 0)
	{
		if(*is_null)
			temp_bind.is_null = (my_bool*) is_null;
		else
			temp_bind.buffer = &param;

	}
	else
		temp_bind.buffer = &param;
	
	bindme.push_back(temp_bind);
}

void MysqlCDR::tempdump_record()
{

}

void MysqlCDR::reread_tempdumped_records()
{

}

std::string MysqlCDR::get_display_name()
{
	return display_name;
}

bool MysqlCDR::process_record()
{
	switch_time_exp_t tm1,tm2,tm3,tm4; // One for call start, answer, transfer, and end
	memset(&tm1,0,sizeof(tm1));
	memset(&tm2,0,sizeof(tm2));
	memset(&tm3,0,sizeof(tm3));
	memset(&tm4,0,sizeof(tm4));
	
	convert_time(&tm1,callstartdate);
	convert_time(&tm2,callanswerdate);
	convert_time(&tm3,calltransferdate);
	convert_time(&tm4,callenddate);
	
	set_mysql_time(tm1,my_callstartdate);
	set_mysql_time(tm2,my_callanswerdate);
	set_mysql_time(tm3,my_calltransferdate);
	set_mysql_time(tm4,my_callenddate);
	
	// Why is this out of order?  I don't know, it doesn't make sense.
	add_parameter(my_callstartdate,MYSQL_TYPE_DATETIME);
	add_parameter(my_callanswerdate,MYSQL_TYPE_DATETIME);
	add_parameter(my_calltransferdate,MYSQL_TYPE_DATETIME);
	add_parameter(my_callenddate,MYSQL_TYPE_DATETIME);
	
	add_parameter(originated,MYSQL_TYPE_TINY);
	add_string_parameter(clid,clid_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(src,src_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(dst,dst_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(ani,ani_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(aniii,aniii_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(dialplan,dialplan_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(myuuid,myuuid_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(destuuid,destuuid_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(srcchannel,srcchannel_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(dstchannel,dstchannel_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(network_addr,network_addr_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(lastapp,lastapp_length,MYSQL_TYPE_VAR_STRING,0);
	add_string_parameter(lastdata,lastdata_length,MYSQL_TYPE_VAR_STRING,0);
	add_parameter(billusec,MYSQL_TYPE_LONGLONG,0);
	add_parameter(disposition,MYSQL_TYPE_TINY,0);
	add_parameter(hangupcause,MYSQL_TYPE_LONG,0);
	add_parameter(amaflags,MYSQL_TYPE_TINY,0);
		
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
					bool* is_null = new bool;
					*is_null = 0;
					
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> *x;
					}
					else
						*is_null = 1;
					temp_chanvars_holder.push_back(x);
					temp_chanvars_holder.push_back(is_null);
					add_parameter(*x,MYSQL_TYPE_LONG,is_null);
					break;
				}
				case CDR_DOUBLE: 
				{
					double* x = new double;
					*x = 0;
					bool* is_null = new bool;
					*is_null = 0;
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> *x;
					}
					else
						*is_null = 1;
					temp_chanvars_holder.push_back(x);
					temp_chanvars_holder.push_back(is_null);
					add_parameter(*x,MYSQL_TYPE_DOUBLE,is_null);
					break;
				}
				case CDR_TINY:
				{
					short* x = new short;
					*x = 0;
					bool* is_null = new bool;
					*is_null = 0;
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> *x;
					}
					else
						*is_null = 1;
					temp_chanvars_holder.push_back(x);
					temp_chanvars_holder.push_back(is_null);
					add_parameter(*x,MYSQL_TYPE_TINY,is_null);
					break;
				}
				case CDR_STRING:
				case CDR_DECIMAL:
				{
					long unsigned int* stringlength = new long unsigned int;
					*stringlength = (long unsigned int)(iItr->second.size());
				
					char* x = new char[(*stringlength+1)];
					strncpy(x,iItr->second.c_str(),*stringlength);
					
					bool* is_null = new bool;
					*is_null = 0;
					add_string_parameter(x,*stringlength,MYSQL_TYPE_VAR_STRING,is_null);
				
					temp_chanvars_holder.push_back(stringlength);
					temp_chanvars_holder.push_back(x);
					temp_chanvars_holder.push_back(is_null);
					break;
				}
				default:
					switch_console_printf(SWITCH_CHANNEL_LOG,"We should not get to this point in this switch/case statement.\n");
			}
			i++;
		}
	}
		
	MYSQL_BIND *bindmetemp;
	bindmetemp = new MYSQL_BIND[bindme.size()];
	copy(bindme.begin(), bindme.end(), bindmetemp);
	
	for(int mysql_ping_result = -1, count = 0, mysql_stmt_error_code = -1; mysql_ping_result != 0 && count < 5 && mysql_stmt_error_code != 0 ; count++)
	{
		mysql_ping_result = mysql_ping(conn);
		if(mysql_ping_result)
		{
			switch(mysql_ping_result)
			{
				case CR_SERVER_GONE_ERROR:
				case CR_SERVER_LOST:
				{
					switch_console_printf(SWITCH_CHANNEL_LOG,"We lost connection to the MySQL server.  Trying to reconnect.\n");
					connect_to_database();
					break;
				}
				default:
				{
					switch_console_printf(SWITCH_CHANNEL_LOG,"We have encountered an unknown error when pinging the MySQL server.  Attempting to reconnect anyways.\n");
					connect_to_database();
				}
			}
		}
		else
		{
			mysql_stmt_bind_param(stmt,bindmetemp);
			mysql_stmt_error_code = mysql_stmt_execute(stmt);
			
			if(mysql_stmt_error_code != 0)
			{
				errorstate = 1;
				switch_console_printf(SWITCH_CHANNEL_LOG,"MysqlCDR::process_record() - Statement executed? Error: %d\n",mysql_stmt_error_code);
			
				const char* mysql_stmt_error_string = mysql_stmt_error(stmt);
				switch_console_printf(SWITCH_CHANNEL_LOG,"MySQL encountered error: %s\n",mysql_stmt_error_string);
			}
			else
				errorstate = 0;
			
			if(logchanvars && chanvars_supp.size() > 0 && errorstate == 0)
			{
				long long insertid = mysql_stmt_insert_id(stmt);

				std::map<std::string,std::string>::iterator iItr,iBeg,iEnd;
				iEnd = chanvars_supp.end();
				for(iItr = chanvars_supp.begin(); iItr != iEnd; iItr++)
				{
					MYSQL_BIND bindme_chanvars[3];
					memset(bindme_chanvars,0,sizeof(bindme_chanvars));
			
					bindme_chanvars[0].buffer_type = MYSQL_TYPE_LONGLONG;
					bindme_chanvars[0].buffer = &insertid;
			
					std::vector<char> tempfirstvector(iItr->first.begin(), iItr->first.end());
					tempfirstvector.push_back('\0');
					char* varname_temp = &tempfirstvector[0];

					bindme_chanvars[1].buffer_type = MYSQL_TYPE_VAR_STRING;
					long unsigned int varname_length = (long unsigned int)(iItr->first.size());
					bindme_chanvars[1].length = &varname_length;
					bindme_chanvars[1].buffer_length = varname_length;
					bindme_chanvars[1].buffer = varname_temp;
			
					std::vector<char> tempsecondvector(iItr->second.begin(), iItr->second.end());
					tempsecondvector.push_back('\0');
					char* varvalue_temp = &tempsecondvector[0];
			
					bindme_chanvars[2].buffer_type = MYSQL_TYPE_VAR_STRING;
					if(iItr->second.size() == 0)
						bindme_chanvars[2].is_null = (my_bool*)1;
					else
					{
						long unsigned int varvalue_length = (long unsigned int)(iItr->second.size());
						bindme_chanvars[2].length = &varvalue_length;
						bindme_chanvars[2].buffer_length = varvalue_length;
						bindme_chanvars[2].buffer = varvalue_temp;
					}
			
					mysql_stmt_bind_param(stmt_chanvars,bindme_chanvars);
					mysql_stmt_execute(stmt_chanvars);
				}
			}
			
			if(errorstate == 0)
				mysql_commit(conn);
			else
				mysql_rollback(conn);
		}
	}
	
	delete [] bindmetemp;
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
					long unsigned int* stringlength = (long unsigned int*)temp_chanvars_holder.front();
					temp_chanvars_holder.pop_front();
					delete stringlength;
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
			
			bool* tempbool = (bool*) temp_chanvars_holder.front();
			temp_chanvars_holder.pop_front();
			delete tempbool;
		}
	}
	return 1;
}

void MysqlCDR::disconnect()
{
	mysql_stmt_close(stmt);
	if(logchanvars)
		mysql_stmt_close(stmt_chanvars);
	mysql_close(conn);
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
}



void MysqlCDR::add_string_parameter(char* param, long unsigned int& param_length, enum_field_types type, bool *is_null)
{
	MYSQL_BIND temp_bind;
	memset(&temp_bind,0,sizeof(temp_bind));
	temp_bind.buffer_type = type;
	if(is_null != 0)
	{
		if(*is_null || param == 0)
			temp_bind.is_null = (my_bool*) is_null;
		else
		{
			temp_bind.length = &param_length;
			temp_bind.buffer_length = param_length;
			temp_bind.buffer = param;
		}
	}
	else
	{
		temp_bind.length = &param_length;
		temp_bind.buffer_length = param_length;
		temp_bind.buffer = param;
	}

	bindme.push_back(temp_bind);
}

void MysqlCDR::set_mysql_time(switch_time_exp_t& param, MYSQL_TIME& destination)
{
	destination.year = param.tm_year + 1900;
	destination.month = param.tm_mon + 1;
	destination.day = param.tm_mday;
	destination.hour = param.tm_hour;
	destination.minute = param.tm_min;
	destination.second = param.tm_sec;	
}

AUTO_REGISTER_BASECDR(MysqlCDR);

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
