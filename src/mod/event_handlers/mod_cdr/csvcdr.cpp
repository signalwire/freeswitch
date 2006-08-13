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
 * Description: This C++ source file describes the CsvCDR class that handles processing CDRs to a CSV format.
 * This is the standard CSV module, and has a list of predefined variables to log out which can be
 * added to, but never have the default ones removed.  If you want to use one that allows you to explicity
 * set all data variables to be logged and in what order, then this is not the class you want to use, and
 * one will be coming in the future to do just that.
 *
 * csvcdr.cpp
 *
 */

#include <switch.h>
#include "csvcdr.h"
#include <string>


CsvCDR::CsvCDR() : BaseCDR()
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcallenddate,0,100);
}

CsvCDR::CsvCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcallenddate,0,100);
	
	if(newchannel != 0)
	{
		switch_time_exp_t tempcallstart, tempcallanswer, tempcallend;
		memset(&tempcallstart,0,sizeof(tempcallstart));
		memset(&tempcallanswer,0,sizeof(tempcallanswer));
		memset(&tempcallend,0,sizeof(tempcallend));
		switch_time_exp_lt(&tempcallstart, callstartdate);
		switch_time_exp_lt(&tempcallanswer, callanswerdate);
		switch_time_exp_lt(&tempcallend, callenddate);
		
		// Format the times
		apr_size_t retsizecsd, retsizecad, retsizeced;  //csd == callstartdate, cad == callanswerdate, ced == callenddate, ceff == callenddate_forfile
		char format[] = "%Y-%m-%d-%H-%M-%S";
		switch_strftime(formattedcallstartdate,&retsizecsd,sizeof(formattedcallstartdate),format,&tempcallstart);
		switch_strftime(formattedcallanswerdate,&retsizecad,sizeof(formattedcallanswerdate),format,&tempcallanswer);
		switch_strftime(formattedcallenddate,&retsizeced,sizeof(formattedcallenddate),format,&tempcallend);

		process_channel_variables(chanvars_fixed_list,newchannel->channel);
		process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,repeat_fixed_in_supp);
	}
}

CsvCDR::~CsvCDR()
{

}

bool CsvCDR::activated=0;
bool CsvCDR::logchanvars=0;
bool CsvCDR::connectionstate=0;
bool CsvCDR::repeat_fixed_in_supp=0;
std::string CsvCDR::outputfile_path;
std::ofstream CsvCDR::outputfile;
std::ofstream::pos_type CsvCDR::filesize_limit = (10 * 1024 * 1024); // Default file size is 10MB
std::list<std::string> CsvCDR::chanvars_fixed_list;
std::list<std::string> CsvCDR::chanvars_supp_list;

void CsvCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	switch_console_printf(SWITCH_CHANNEL_LOG, "CsvCDR::connect() - Loading configuration file.\n");
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.
	
	if ((settings = switch_xml_child(cfg, "csvcdr"))) 
	{
		int count_config_params = 0;  // Need to make sure all params are set before we load
		for (param = switch_xml_child(settings, "param"); param; param = param->next) 
		{
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "path"))
			{
				if(val != 0)
					outputfile_path = val;
				count_config_params++;
			}
			else if (!strcmp(var, "chanvars_supp")) 
			{
				if(val != 0)
				{
					std::string unparsed;
					unparsed = val;
					if(unparsed.size() > 0)
					{
						bool fixed = 0;
						parse_channel_variables_xconfig(unparsed,chanvars_supp_list,fixed);
						logchanvars=1;
					}
				}
			}
			else if (!strcmp(var, "chanvars_fixed"))
			{
				if(val != 0)
				{
					std::string unparsed;
					unparsed = val;
					if(unparsed.size() > 0)
					{
						bool fixed = 1;
						parse_channel_variables_xconfig(unparsed,chanvars_fixed_list,fixed);
						logchanvars=1;
					}
				}
			}
			else if (!strcmp(var, "repeat_fixed_in_supp"))
			{
				if(!strcmp(val,"1"))
					repeat_fixed_in_supp = 1;
				else if (!strcmp(val,"y") || !strcmp(val,"y"))
					repeat_fixed_in_supp = 0;
			}
			else if (!strcmp(var, "size_limit"))
			{
				if(val != 0)
				{
					filesize_limit = atoi(val) * 1024 * 1024; // Value is in MB
					std::cout << "File size limit from config file is " << filesize_limit << " byte(s)." << std::endl;
				}
			}
		}
		
		if(count_config_params > 0)
		{
			open_file();
			if(outputfile.good())
			{
				activated = 1;
				switch_console_printf(SWITCH_CHANNEL_LOG,"CsvCDR activated, log rotation will occur at or after %d MB",(filesize_limit/1024/1024));
			}
		}
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"CsvCDR::connect(): You did not specify the minimum parameters for using this module.  You must specify at least a path to have the records logged to.\n");
	}
}

void CsvCDR::check_file_size_and_open()
{
	// Test if the file has been opened or not
	if(outputfile)
	{
		if(outputfile.tellp() > filesize_limit)
			outputfile.close();
	}
	
	if (!outputfile)
	{
		open_file();
	}
}

void CsvCDR::open_file()
{
	switch_time_t now = switch_time_now();
	switch_time_exp_t now_converted;
	memset(&now_converted,0,sizeof(now_converted));
		
	switch_time_exp_lt(&now_converted,now);
		
	apr_size_t retsize;		
	char format[] = "%Y-%m-%d-%H-%M-%S";
	char formatteddate[100];
	memset(formatteddate,0,100);
	switch_strftime(formatteddate,&retsize,100,format,&now_converted);
		
	std::string filename = outputfile_path;
	filename.append(SWITCH_PATH_SEPARATOR);
	filename.append(formatteddate);
	filename.append(".csv");
	outputfile.clear();
		
	outputfile.open(filename.c_str(),std::ios_base::app|std::ios_base::binary);
	if(outputfile.fail())
	{
		switch_console_printf(SWITCH_CHANNEL_LOG,"Could not open the CSV file %s .  CsvCDR logger will not be functional until this is resolved and a reload is issued.  Failbit is set to %d.\n",filename.c_str(),outputfile.fail());
		activated = 0;
	}
	
}

bool CsvCDR::process_record()
{
	check_file_size_and_open();
	bool retval = 0;
	
	// Format the call record and proceed from here...
	outputfile << "\"" << callstartdate << "\",\"";
	outputfile << callanswerdate << "\",\"";
	outputfile << callenddate << "\",\"";
	outputfile << hangupcause_text << "\",\"";
	outputfile << hangupcause << "\",\"";
	outputfile << clid << "\",\"";
	outputfile << originated << "\",\"";
	outputfile << dialplan << "\",\"";
	outputfile << myuuid << "\",\"";
	outputfile << destuuid << "\",\"";
	outputfile << src << "\",\"";
	outputfile << dst << "\",\"";
	outputfile << srcchannel << "\",\"";
	outputfile << dstchannel << "\",\"";
	outputfile << ani << "\",\"";
	outputfile << ani2 << "\",\"";
	outputfile << network_addr << "\",\"";
	outputfile << lastapp << "\",\"";
	outputfile << lastdata << "\",\"";
	outputfile << billusec << "\",\"";
	outputfile << disposition << "\",\"";
	outputfile << amaflags << "\"";
	
	// Now to process chanvars, fixed ones first
	if(chanvars_fixed.size() > 0 )
	{
		std::list<std::pair<std::string,std::string> >::iterator iItr, iEnd;
		for(iItr = chanvars_fixed.begin(), iEnd = chanvars_fixed.end(); iItr != iEnd; iItr++)
			outputfile << ",\"" << iItr->second << "\"";
	}
		
	if(chanvars_supp.size() > 0 )
	{
		std::map<std::string,std::string>::iterator iItr,iEnd;
		for(iItr = chanvars_supp.begin(), iEnd = chanvars_supp.end() ; iItr != iEnd; iItr++)
			outputfile << ",\"" << iItr->first << "=" << iItr->second << "\"";
	}
	outputfile << std::endl;
	retval = 1;
		
	return retval;
}

bool CsvCDR::is_activated()
{
	return activated;
}

void CsvCDR::tempdump_record()
{

}

void CsvCDR::reread_tempdumped_records()
{

}

void CsvCDR::disconnect()
{
	outputfile.close();
	switch_console_printf(SWITCH_CHANNEL_LOG,"Shutting down CsvCDR...  Done!");	
}

AUTO_REGISTER_BASECDR(CsvCDR);
