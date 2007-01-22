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
 * Description: his C++ header file describes the SqliteCDR class which handles formatting a CDR out to
 * a SQLite database using prepared statements.
 *
 * sqlitecdr.cpp
 *
 */

#include <switch.h>
#include <cstring>
#include <iostream>
#include "sqlitecdr.h"

SqliteCDR::SqliteCDR() : BaseCDR()
{

}

SqliteCDR::SqliteCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
{
	if(newchannel != 0)
	{
		if(chanvars_fixed_list.size() > 0)
			process_channel_variables(chanvars_fixed_list,newchannel->channel);
		
		if(chanvars_supp_list.size() > 0)
			process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,repeat_fixed_in_supp);
	}
}

SqliteCDR::~SqliteCDR()
{

}

bool SqliteCDR::connectionstate = 0;
bool SqliteCDR::logchanvars = 0;
bool SqliteCDR::repeat_fixed_in_supp = 0;
std::list<std::string> SqliteCDR::chanvars_fixed_list;
std::list<std::string> SqliteCDR::chanvars_supp_list;
std::vector<switch_mod_cdr_sql_types_t> SqliteCDR::chanvars_fixed_types;
bool SqliteCDR::activated = 0;
char SqliteCDR::sql_query[1024] = "";
std::string SqliteCDR::tmp_sql_query;
char SqliteCDR::sql_query_chanvars[100] = "";
std::string SqliteCDR::db_filename;
switch_core_db_t* SqliteCDR::db = 0;
switch_core_db_stmt_t* SqliteCDR::stmt=0;
switch_core_db_stmt_t* SqliteCDR::stmt_chanvars=0;
switch_core_db_stmt_t* SqliteCDR::stmt_begin=0;
switch_core_db_stmt_t* SqliteCDR::stmt_commit=0;
bool SqliteCDR::use_utc_time = 0;
std::string SqliteCDR::display_name = "SqliteCDR - The sqlite3 cdr logging backend";

void SqliteCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	if(activated)
		disconnect();
	
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.
	
	int count_config_params = 0;  // Need to make sure all params are set before we load
	if ((settings = switch_xml_child(cfg, "sqlitecdr"))) 
	{
		for (param = switch_xml_child(settings, "param"); param; param = param->next) 
		{
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "path"))
			{
				if(val != 0)
				{
					db_filename = val;
					db_filename.append(SWITCH_PATH_SEPARATOR);
					db_filename.append("sqlitecdr.db");
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
					use_utc_time = 1;
				else if(!strcmp(val,"local"))
					use_utc_time = 0;
				else
				{
					switch_console_printf(SWITCH_CHANNEL_LOG,"Invalid configuration parameter for timezone.  Possible values are utc and local.  You entered: %s\nDefaulting to local.\n",val);
					use_utc_time = 0;
				}
			}
		}
		
		if (count_config_params==1)
			activated = 1;
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"You did not specify the minimum parameters for using this module.  You must specify an explicit (complete) path to the location of the database file in order to use SqliteCDR.\n");
		
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
			
			int sql_rc = switch_core_db_open(db_filename.c_str(),&db);
			
			if(sql_rc != SQLITE_OK)
			{
				switch_console_printf(SWITCH_CHANNEL_LOG,"There was an error opening database filename %s.  The error was: %s.  SqliteCDR logging has been disabled until the problem is resolved and modcdr_reload is initiated.\n",db_filename.c_str(),switch_core_db_errmsg(db));
				activated = 0;
				switch_core_db_close(db);
			}
			else
			{
				char sql_query_check_tables[] = "SELECT name FROM sqlite_master WHERE type = \"table\"";
				char **result;
				int nrow = 0, ncol = 0;
				char *errormessage;
				sql_rc = switch_core_db_get_table(db,sql_query_check_tables,&result,&nrow,&ncol,&errormessage);
			
				std::map<std::string,bool> temp_chanvars_map;
				// Now to copy out all the chanvars from the list into 
			
				std::map<std::string,bool> temp_sql_tables;
				temp_sql_tables["freeswitchcdr"] = 0;
				temp_sql_tables["chanvars"] = 0;
			
				if(sql_rc == SQLITE_OK)
				{
					for(int i = 0; i < ((nrow+1)*ncol); i++)
					{
						std::string tablename = result[i];
						if(tablename == "freeswitchcdr" || tablename == "chanvars")
						{
							temp_sql_tables[tablename] = 1;
						}
					}
				}
				else
					switch_console_printf(SWITCH_CHANNEL_LOG,"There was an error in executing query %s: The error was %s.\n",sql_query_check_tables,errormessage);
				
				switch_core_db_free_table(result);
			
				if(!temp_sql_tables["freeswitchcdr"])
				{
					switch_console_printf(SWITCH_CHANNEL_LOG,"Creating the freeswitchcdr table in the SQLite mod_cdr database file.\n");
					// Must create the missing freeswitchcdr table.
					char sql_query_create_freeswitchcdr[] = "CREATE TABLE freeswitchcdr (\n"
						"callid INTEGER PRIMARY KEY AUTOINCREMENT,\n"
						"callstartdate INTEGER NOT NULL,\n"
						"callanswerdate INTEGER NOT NULL,\n"
						"calltransferdate INTEGER NOT NULL,\n"
						"callenddate INTEGER NOT NULL,\n"
						"originated INTEGER default 0,\n"
						"clid TEXT default \"Freeswitch - Unknown\",\n"
						"src TEXT NOT NULL,\n"
						"dst TEXT NOT NULL,\n"
						"ani TEXT default \"\",\n"
						"aniii TEXT default \"\",\n"
						"dialplan TEXT default \"\",\n"
						"myuuid TEXT NOT NULL,\n"
						"destuuid TEXT NOT NULL,\n"
						"srcchannel TEXT NOT NULL,\n"
						"dstchannel TEXT NOT NULL,\n"
						"network_addr TEXT,\n"
						"lastapp TEXT default \"\",\n"
						"lastdata TEXT default \"\",\n"
						"billusec INTEGER default 0,\n"
						"disposition INTEGER default 0,\n"
						"hangupcause INTEGER default 0,\n"
						"amaflags INTEGER default 0\n"
						");\n";
						
					switch_core_db_exec(db, sql_query_create_freeswitchcdr, NULL, NULL, NULL);
				}
			
				if(!temp_sql_tables["chanvars"])
				{
					switch_console_printf(SWITCH_CHANNEL_LOG,"Creating the chanvars table in the SQLite mod_cdr database file.\n");
					// Must create the missing chanvars table.
					char sql_query_create_chanvars[] = "CREATE TABLE chanvars (\n"
						"callid INTEGER default 0,\n"
						"varname TEXT NOT NULL,\n"
						"varvalue TEXT default \"\"\n"
						");\n";
					
					switch_core_db_exec(db, sql_query_create_chanvars, NULL, NULL, NULL);
				}
				
				if(chanvars_fixed_list.size() > 0)
				{
					// Now to check if the freeswitchcdr schema matches
					std::map<std::string,std::string> freeswitchcdr_columns;
					char sql_query_get_schema_of_freeswitchcdr[] = "SELECT sql FROM SQLITE_MASTER WHERE name=\"freeswitchcdr\"";
					char **result2;
					nrow = 0;
					ncol = 0;
					char *errormessage2;
					sql_rc = switch_core_db_get_table(db,sql_query_get_schema_of_freeswitchcdr,&result2,&nrow,&ncol,&errormessage2);
					
					if(sql_rc == SQLITE_OK)
					{
						for(int k = 0; k < nrow; k++)
						{
							// Warning - this is slightly ugly for string parsing
							std::string resultstring = result2[1];
							std::string::size_type j = resultstring.find('(',0);
							j = resultstring.find('\n',j);
							
							std::string::size_type h = 0;
							
							std::string tempstring1,tempstring2;
							
							for(std::string::size_type i = j+1 ; j != std::string::npos; )
							{
								j = resultstring.find(' ',i);
								if(j > 0)
								{
									if(j == i)
									{
										i++;
										j = resultstring.find(' ',(i));
									}
									
									tempstring1 = resultstring.substr(i,(j-i));
									i = j+1;
									j =resultstring.find(',',i);
									
									h = resultstring.find(' ',i);
									if(j == std::string::npos)
										tempstring2 = resultstring.substr(i,(resultstring.size() - i));
									else if(j > h)
										tempstring2 = resultstring.substr(i,(h-i));
									else
										tempstring2 = resultstring.substr(i,(j-i));
								
									freeswitchcdr_columns[tempstring1] = tempstring2;
									// switch_console_printf(SWITCH_CHANNEL_LOG,"tempstring1 = %s, tempstring2 = %s\n",tempstring1.c_str(),tempstring2.c_str());
									if(resultstring.find('\n',j+1) == (j+1))
										j++;
									i = j+1;
								}
								else
									switch_console_printf(SWITCH_CHANNEL_LOG,"There has been a parsing problem with the freeswitchcdr schema.\n");
							}
						}
					}
					
					switch_core_db_free_table(result2);
					
					// Now to actually compare what we have in the config against the db schema
					std::map<std::string,std::string> freeswitchcdr_add_columns;
					std::list<std::string>::iterator iItr, iEnd;
					switch_size_t i = 0;
					for(iItr = chanvars_fixed_list.begin(), iEnd = chanvars_fixed_list.end(); iItr != iEnd; iItr++, i++)
					{
						switch(chanvars_fixed_types[i])
						{
							case CDR_INTEGER:
							case CDR_TINY:
								if(freeswitchcdr_columns.find(*iItr) != freeswitchcdr_columns.end())
								{
									//switch_console_printf(SWITCH_CHANNEL_LOG,"freeswitchcdr_columns[%s] == %s.\n",iItr->c_str(),freeswitchcdr_columns[*iItr].c_str());
									if(freeswitchcdr_columns[*iItr].find("INTEGER",0) == std::string::npos)
										switch_console_printf(SWITCH_CHANNEL_LOG,"WARNING: SqliteCDR freeswitchcdr table column type mismatch: Column \"%s\" is not of an INTEGER type.  This is not necessarily fatal, but may result in unexpected behavior.\n",iItr->c_str());
								}
								else
									freeswitchcdr_add_columns[*iItr] = "INTEGER";
								break;
							case CDR_DOUBLE:
								if(freeswitchcdr_columns.find(*iItr) != freeswitchcdr_columns.end())
								{
									if(freeswitchcdr_columns[*iItr].find("REAL",0) == std::string::npos)
										switch_console_printf(SWITCH_CHANNEL_LOG,"WARNING: SqliteCDR freeswitchcdr table column type mismatch: Column \"%s\" is not of a REAL type.  This is not necessarily fatal, but may result in unexpected behavior.\n",iItr->c_str());
								}
								else
									freeswitchcdr_add_columns[*iItr] = "REAL";
								break;
							case CDR_DECIMAL:
							case CDR_STRING:
								if(freeswitchcdr_columns.find(*iItr) != freeswitchcdr_columns.end())
								{
									if(freeswitchcdr_columns[*iItr].find("TEXT",0) == std::string::npos)
										switch_console_printf(SWITCH_CHANNEL_LOG,"WARNING: SqliteCDR freeswitchcdr table column type mismatch: Column \"%s\" is not of a TEXT type.  This is not necessarily fatal, but may result in unexpected behavior.\n",iItr->c_str());
								}
								else
									freeswitchcdr_add_columns[*iItr] = "TEXT";
								break;
							default:
								switch_console_printf(SWITCH_CHANNEL_LOG,"Oh bother, I should not have fallen into this hole in the switch/case statement.  Please notify the author.\n");
						}
					}
					
					if(freeswitchcdr_add_columns.size())
					{
						switch_console_printf(SWITCH_CHANNEL_LOG,"Updating the freeswitchcdr table schema.\n");
						std::string tempsql_freeswitchcdr_alter_table = "ALTER TABLE freeswitchcdr ADD ";
						std::map<std::string, std::string>::iterator iItr, iEnd;
						for(iItr = freeswitchcdr_add_columns.begin(), iEnd = freeswitchcdr_add_columns.end(); iItr != iEnd; iItr++)
						{
							std::string sql_query_freeswitchcdr_alter_table = tempsql_freeswitchcdr_alter_table;
							sql_query_freeswitchcdr_alter_table.append(iItr->first);
							sql_query_freeswitchcdr_alter_table.append(" ");
							sql_query_freeswitchcdr_alter_table.append(iItr->second);
							switch_console_printf(SWITCH_CHANNEL_LOG,"Updating the freeswitchcdr table with the following SQL command: %s.\n",sql_query_freeswitchcdr_alter_table.c_str());
							switch_core_db_exec(db, sql_query_freeswitchcdr_alter_table.c_str(), NULL, NULL, NULL);
						}
					}
				}
				
				switch_core_db_prepare(db,"BEGIN TRANSACTION SqliteCDR",-1,&stmt_begin,0);
				switch_core_db_prepare(db,"COMMIT TRANSACTION SqliteCDR",-1,&stmt_commit,0);
				switch_core_db_prepare(db,tmp_sql_query.c_str(),-1,&stmt,0);
				if(chanvars_supp_list.size())
					switch_core_db_prepare(db,sql_query_chanvars,-1,&stmt_chanvars,0);
			}
		}
	}
}

bool SqliteCDR::is_activated()
{
	return activated;
}

void SqliteCDR::tempdump_record()
{

}

void SqliteCDR::reread_tempdumped_records()
{

}


bool SqliteCDR::process_record()
{
	if(use_utc_time)
	{
		switch_time_exp_t tm1, tm2, tm3, tm4;
		memset(&tm1,0,sizeof(tm1));
		memset(&tm2,0,sizeof(tm2));
		memset(&tm3,0,sizeof(tm3));
		memset(&tm4,0,sizeof(tm4));
	
		switch_time_exp_gmt(&tm1,callstartdate);
		switch_time_exp_gmt(&tm2,callanswerdate);
		switch_time_exp_gmt(&tm3,calltransferdate);
		switch_time_exp_gmt(&tm4,calltransferdate);
		
		switch_time_exp_gmt_get(&sqlite_callstartdate,&tm1);
		switch_time_exp_gmt_get(&sqlite_callanswerdate,&tm2);
		switch_time_exp_gmt_get(&sqlite_calltransferdate,&tm3);
		switch_time_exp_gmt_get(&sqlite_callenddate,&tm4);
	}
	else
	{
		sqlite_callstartdate = callstartdate;
		sqlite_callanswerdate = callanswerdate;
		sqlite_calltransferdate = calltransferdate;
		sqlite_callenddate = callenddate;
	}
	
	int column = 1;
	switch_core_db_step(stmt_begin);
	switch_core_db_reset(stmt_begin);
	switch_core_db_bind_int64(stmt, column++, (sqlite_int64) sqlite_callstartdate);
	switch_core_db_bind_int64(stmt, column++, (sqlite_int64) sqlite_callanswerdate);
	switch_core_db_bind_int64(stmt, column++, (sqlite_int64) sqlite_calltransferdate);
	switch_core_db_bind_int64(stmt, column++, (sqlite_int64) sqlite_callenddate);
	switch_core_db_bind_int(stmt, column++, (int) originated);
	switch_core_db_bind_text(stmt, column++, clid,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, src,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, dst,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, ani,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, aniii,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, dialplan,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, myuuid,36,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, destuuid,36,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, srcchannel,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, dstchannel,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, network_addr,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, lastapp,-1,SQLITE_STATIC);
	switch_core_db_bind_text(stmt, column++, lastdata,-1,SQLITE_STATIC);
	switch_core_db_bind_int64(stmt, column++, (sqlite_int64) billusec);
	switch_core_db_bind_int(stmt, column++, disposition);
	switch_core_db_bind_int(stmt, column++, (int) hangupcause);
	switch_core_db_bind_int(stmt, column++, amaflags);
	
	if(chanvars_fixed.size())
	{
		std::list< std::pair<std::string,std::string> >::iterator iItr, iEnd;
		int count = 0;
		for(iItr = chanvars_fixed.begin(), iEnd = chanvars_fixed.end(); iItr != iEnd; iItr++, count++)
		{
			switch(chanvars_fixed_types[count])
			{
				case CDR_INTEGER:
				case CDR_TINY:
				{
					int x;
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> x;
					}
					else
						x = 0;
					switch_core_db_bind_int(stmt,column++,x);
					break;
				}
				case CDR_DOUBLE:
				{
					double x = 0;
					if(iItr->second.size() > 0)
					{
						std::istringstream istring(iItr->second);
						istring >> x;
					}
					switch_core_db_bind_double(stmt,column++,x);
					break;
				}
				case CDR_DECIMAL:
				case CDR_STRING:
				{
					switch_core_db_bind_text(stmt,column++,iItr->second.c_str(),-1,SQLITE_STATIC);
					break;
				}
				default:
					switch_console_printf(SWITCH_CHANNEL_LOG,"Oh bother, I should not have fallen into this hole in the switch/case statement.  Please notify the author.\n");
			}
		}
	}
	
	int sql_rc = switch_core_db_step(stmt);
	if(sql_rc != SQLITE_DONE)
	{
		if(sql_rc == SQLITE_BUSY)
			sql_rc = switch_core_db_step(stmt);
		else if (sql_rc == SQLITE_ERROR || sql_rc == SQLITE_MISUSE)
			switch_console_printf(SWITCH_CHANNEL_LOG,"There was an error executing switch_core_db_step on SqliteCDR::stmt.  The error was: %s\n",switch_core_db_errmsg(db));
	}
	
	sql_rc = switch_core_db_reset(stmt);
	
	if(logchanvars && chanvars_supp.size())
	{
		sqlite_int64 rowid = switch_core_db_last_insert_rowid(db);
		int column2 = 1;
		std::map<std::string,std::string>::iterator iItr, iEnd;
		for(iItr = chanvars_supp.begin(), iEnd = chanvars_supp.end(); iItr != iEnd; iItr++)
		{
			switch_core_db_bind_int64(stmt_chanvars, column2++, rowid);
			switch_core_db_bind_text(stmt_chanvars, column2++, iItr->first.c_str(),-1,SQLITE_STATIC);
			switch_core_db_bind_text(stmt_chanvars, column2++, iItr->second.c_str(),-1,SQLITE_STATIC);
			int sql_rc = switch_core_db_step(stmt_chanvars);
			if(sql_rc != SQLITE_DONE)
			{
				if(sql_rc == SQLITE_BUSY)
					sql_rc = switch_core_db_step(stmt_chanvars);
				else if (sql_rc == SQLITE_ERROR || sql_rc == SQLITE_MISUSE)
					switch_console_printf(SWITCH_CHANNEL_LOG,"There was an error executing switch_core_db_step on SqliteCDR::stmt_chanvars.  The error was: %s\n",switch_core_db_errmsg(db));
			}
			
			switch_core_db_reset(stmt_chanvars);
		}
	}
	
	switch_core_db_step(stmt_commit);
	switch_core_db_reset(stmt_commit);
	
	return 1;
}

void SqliteCDR::disconnect()
{
	switch_core_db_finalize(stmt_chanvars);
	switch_core_db_finalize(stmt);
	switch_core_db_finalize(stmt_begin);
	switch_core_db_finalize(stmt_commit);
	switch_core_db_close(db);
	activated = 0;
	logchanvars = 0;
	chanvars_fixed_list.clear();
	chanvars_supp_list.clear();
	chanvars_fixed_types.clear();
	connectionstate = 0;
	tmp_sql_query.clear();
}

std::string SqliteCDR::get_display_name()
{
	return display_name;
}



AUTO_REGISTER_BASECDR(SqliteCDR);

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
