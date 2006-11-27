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
 * Description: This C++ source file describes the PddCDR class which handles formatting a CDR out to
 * individual text files in a Perl Data Dumper format.
 *
 * pddcdr.cpp
 *
 */

#include <switch.h>
#include "pddcdr.h"

PddCDR::PddCDR() : BaseCDR()
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcallenddate,0,100);
}

PddCDR::PddCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
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
		size_t retsizecsd, retsizecad, retsizeced; //csd == callstartdate, cad == callanswerdate, ced == callenddate, ceff == callenddate_forfile
		char format[] = "%Y-%m-%d-%H-%M-%S";
		switch_strftime(formattedcallstartdate,&retsizecsd,sizeof(formattedcallstartdate),format,&tempcallstart);
		switch_strftime(formattedcallanswerdate,&retsizecad,sizeof(formattedcallanswerdate),format,&tempcallanswer);
		switch_strftime(formattedcallenddate,&retsizeced,sizeof(formattedcallenddate),format,&tempcallend);

		std::ostringstream ostring;
		ostring << (callenddate/1000000);
		std::string callenddate_forfile = ostring.str();
		
		outputfile_name = outputfile_path;
		outputfile_name.append(SWITCH_PATH_SEPARATOR);
		outputfile_name.append(callenddate_forfile); // Make sorting a bit easier, kinda like Maildir does
		outputfile_name.append(".");
		outputfile_name.append(myuuid);  // The goal is to have a resulting filename of "/path/to/myuuid"
		outputfile_name.append(".pdd");  // .pdd - "perl data dumper"
	
		outputfile.open(outputfile_name.c_str());
	
		bool repeat = 1;
		process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,repeat);
	}
}

PddCDR::~PddCDR()
{
	outputfile.close();	
}

bool PddCDR::activated=0;
bool PddCDR::logchanvars=0;
bool PddCDR::connectionstate=0;
std::string PddCDR::outputfile_path;
std::list<std::string> PddCDR::chanvars_fixed_list;
std::list<std::string> PddCDR::chanvars_supp_list;

void PddCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	switch_console_printf(SWITCH_CHANNEL_LOG, "PddCDR::connect() - Loading configuration file.\n");
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.
	
	if ((settings = switch_xml_child(cfg, "pddcdr"))) 
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
			else if (!strcmp(var, "chanvars")) 
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
				switch_console_printf(SWITCH_CHANNEL_LOG,"PddCDR has no need for a fixed or supplemental list of channel variables due to the nature of the format.  Please use the setting parameter of \"chanvars\" instead and try again.\n");
			}
			else if (!strcmp(var, "chanvars_supp"))
			{
				switch_console_printf(SWITCH_CHANNEL_LOG,"PddCDR has no need for a fixed or supplemental list of channel variables due to the nature of the format.  Please use the setting parameter of \"chanvars\" instead and try again.\n");
			}
		}
		
		if(count_config_params > 0)
			activated = 1;
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"PddCDR::connect(): You did not specify the minimum parameters for using this module.  You must specify at least a path to have the records logged to.\n");
	}
}

bool PddCDR::process_record()
{
	bool retval = 0;
	if(!outputfile)
		switch_console_printf(SWITCH_CHANNEL_LOG, "PddCDR::process_record():  Unable to open file  %s to commit the call record to.  Invalid path name, invalid permissions, or no space available?\n",outputfile_name.c_str());
	else
	{
		// Format the call record and proceed from here...
		outputfile << "$VAR1 = {" << std::endl;
		outputfile << "\t\'callstartdate\' = \'" << callstartdate << "\'," << std::endl;
		outputfile << "\t\'callanswerdate\' = \'" << callanswerdate << "\'," << std::endl;
		outputfile << "\t\'callenddate\' = \'" << callenddate << "\'," << std::endl;
		outputfile << "\t\'hangupcause\' = \'" << hangupcause_text << "\'," << std::endl;
		outputfile << "\t\'hangupcausecode\' = \'" << hangupcause << "\'," << std::endl;
		outputfile << "\t\'clid\' = \'" << clid << "\'," << std::endl;
		outputfile << "\t\'originated\' = \'" << originated << "\'," << std::endl;
		outputfile << "\t\'dialplan\' = \'" << dialplan << "\'," << std::endl;
		outputfile << "\t\'myuuid\' = \'" << myuuid << "\'," << std::endl;
		outputfile << "\t\'destuuid\' = \'" << destuuid << "\'," << std::endl;
		outputfile << "\t\'src\' = \'" << src << "\'," << std::endl;
		outputfile << "\t\'dst\' = \'" << dst << "\'," << std::endl;
		outputfile << "\t\'srcchannel\' = \'" << srcchannel << "\'," << std::endl;
		outputfile << "\t\'dstchannel\' = \'" << dstchannel << "\'," << std::endl;
		outputfile << "\t\'ani\' = \'" << ani << "\'," << std::endl;
		outputfile << "\t\'aniii\' = \'" << aniii << "\'," << std::endl;
		outputfile << "\t\'network_addr\' = \'" << network_addr << "\'," << std::endl;
		outputfile << "\t\'lastapp\' = \'" << lastapp << "\'," << std::endl;
		outputfile << "\t\'lastdata\' = \'" << lastdata << "\'," << std::endl;
		outputfile << "\t\'billusec\' = \'" << billusec << "\'," << std::endl;
		outputfile << "\t\'disposition\' = \'" << disposition << "\'," << std::endl;
		outputfile << "\t\'amaflags\' = \'" << amaflags << "\'," << std::endl;
		
		// Now to process chanvars
		outputfile << "\t\'chanvars\' => {" << std::endl;
		if(chanvars_supp.size() > 0 )
		{
			std::map<std::string,std::string>::iterator iItr,iEnd;
			for(iItr = chanvars_supp.begin(), iEnd = chanvars_supp.end() ; iItr != iEnd; iItr++)
				outputfile << "\t\t\'" << iItr->first << "\' = \'" << iItr->second << "\'," << std::endl;
		}
		outputfile << "\t}," << std::endl << "};" << std::endl << std::endl;
		retval = 1;
	}
	
	return retval;
}

bool PddCDR::is_activated()
{
	return activated;
}

void PddCDR::tempdump_record()
{

}

void PddCDR::reread_tempdumped_records()
{

}

void PddCDR::disconnect()
{
	switch_console_printf(SWITCH_CHANNEL_LOG,"Shutting down PddCDR...  Done!");	
}

AUTO_REGISTER_BASECDR(PddCDR);

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
