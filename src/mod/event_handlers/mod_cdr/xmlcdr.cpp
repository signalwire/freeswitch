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
 * Ken Rice of Asteria Solutions Group, INC <ken AT asteriasgi.com>
 * 
 * Description: This C++ source file describes the XmlCDR class which handles formatting a CDR out to
 * individual text files in a XML format.
 *
 * xmlcdr.cpp
 *
 */

#include <switch.h>
#include "xmlcdr.h"

XmlCDR::XmlCDR() : BaseCDR()
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcallenddate,0,100);
}

XmlCDR::XmlCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcallenddate,0,100);
	
	if(newchannel != 0)
	{
		switch_time_exp_t tempcallstart, tempcallanswer, tempcalltransfer, tempcallend;
		memset(&tempcallstart,0,sizeof(tempcallstart));
		memset(&tempcallanswer,0,sizeof(tempcallanswer));
		memset(&tempcallend,0,sizeof(tempcallend));
		memset(&tempcalltransfer,0,sizeof(tempcalltransfer));
		convert_time(&tempcallstart, callstartdate);
		convert_time(&tempcallanswer, callanswerdate);
		convert_time(&tempcalltransfer, calltransferdate);
		convert_time(&tempcallend, callenddate);
		
		// Format the times
		size_t retsizecsd, retsizecad, retsizectd, retsizeced; //csd == callstartdate, cad == callanswerdate, ced == callenddate, ceff == callenddate_forfile
		char format[] = "%Y-%m-%d %H:%M:%S";
		switch_strftime(formattedcallstartdate,&retsizecsd,sizeof(formattedcallstartdate),format,&tempcallstart);
		switch_strftime(formattedcallanswerdate,&retsizecad,sizeof(formattedcallanswerdate),format,&tempcallanswer);
		switch_strftime(formattedcalltransferdate,&retsizectd,sizeof(formattedcalltransferdate),format,&tempcalltransfer);
		switch_strftime(formattedcallenddate,&retsizeced,sizeof(formattedcallenddate),format,&tempcallend);

		std::ostringstream ostring;
		ostring << (callenddate/1000000);
		std::string callenddate_forfile = ostring.str();
		
		outputfile_name = outputfile_path;
		outputfile_name.append(SWITCH_PATH_SEPARATOR);
		outputfile_name.append(callenddate_forfile); // Make sorting a bit easier, kinda like Maildir does
		outputfile_name.append(".");
		outputfile_name.append(myuuid);  // The goal is to have a resulting filename of "/path/to/myuuid"
		outputfile_name.append(".xml");  // .xml - "XML Data Dumper"
		
		bool repeat = 1;
		process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,repeat);
	}
}

XmlCDR::~XmlCDR()
{
	outputfile.close();	
}

bool XmlCDR::activated=0;
bool XmlCDR::logchanvars=0;
bool XmlCDR::connectionstate=0;
modcdr_time_convert_t XmlCDR::convert_time = switch_time_exp_lt;
std::string XmlCDR::outputfile_path;
std::list<std::string> XmlCDR::chanvars_fixed_list;
std::list<std::string> XmlCDR::chanvars_supp_list;
std::string XmlCDR::display_name = "XmlCDR - The rough implementation of XML CDR logger";

void XmlCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	switch_console_printf(SWITCH_CHANNEL_LOG, "XmlCDR::connect() - Loading configuration file.\n");
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.
	
	if ((settings = switch_xml_child(cfg, "xmlcdr"))) 
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
				switch_console_printf(SWITCH_CHANNEL_LOG,"XmlCDR has no need for a fixed or supplemental list of channel variables due to the nature of the format.  Please use the setting parameter of \"chanvars\" instead and try again.\n");
			}
			else if (!strcmp(var, "chanvars_supp"))
			{
				switch_console_printf(SWITCH_CHANNEL_LOG,"XmlCDR has no need for a fixed or supplemental list of channel variables due to the nature of the format.  Please use the setting parameter of \"chanvars\" instead and try again.\n");
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
		
		if(count_config_params > 0)
			activated = 1;
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"XmlCDR::connect(): You did not specify the minimum parameters for using this module.  You must specify at least a path to have the records logged to.\n");
	}
}

bool XmlCDR::process_record()
{
	bool retval = 0;
	outputfile.open(outputfile_name.c_str());
	
	if(!outputfile)
		switch_console_printf(SWITCH_CHANNEL_LOG, "XmlCDR::process_record():  Unable to open file  %s to commit the call record to.  Invalid path name, invalid permissions, or no space available?\n",outputfile_name.c_str());
	else
	{
		switch_console_printf(SWITCH_CHANNEL_LOG, "XmlCDR::process_record():  Preping the CDR to %s.\n",outputfile_name.c_str());
		// Format the call record and proceed from here...
		outputfile << "<?xml version=\"1.0\"?>" << std::endl;
		outputfile << "<document type=\"freeswitch-cdr/xml\">" << std::endl;
		outputfile << "\t<callstartdate data=\"" << callstartdate << "\" />" << std::endl;
		outputfile << "\t<formattedcallstartdate data=\"" << formattedcallstartdate << "\" />" << std::endl;
		outputfile << "\t<callanswerdate data=\"" << callanswerdate << "\" />" << std::endl;
		outputfile << "\t<formattedcallanswerdate data=\"" << formattedcallanswerdate << "\" />" << std::endl;
		outputfile << "\t<calltransferdate data=\"" << calltransferdate << "\" />" << std::endl;
		outputfile << "\t<formattedcalltransferdate data=\"" << formattedcalltransferdate << "\" />" << std::endl;
		outputfile << "\t<callenddate data=\"" << callenddate << "\" />" << std::endl;
		outputfile << "\t<formattedcallenddate data=\"" << formattedcallenddate << "\" />" << std::endl;
		outputfile << "\t<hangupcause data=\"" << hangupcause_text << "\" />" << std::endl;
		outputfile << "\t<hangupcausecode data=\"" << hangupcause << "\" />" << std::endl;
		outputfile << "\t<clid data=\"" << clid << "\" />" << std::endl;
		outputfile << "\t<originated data=\"" << originated << "\" />" << std::endl;
		outputfile << "\t<dialplan data=\"" << dialplan << "\" />" << std::endl;
		outputfile << "\t<myuuid data=\"" << myuuid << "\" />" << std::endl;
		outputfile << "\t<destuuid data=\"" << destuuid << "\" />" << std::endl;
		outputfile << "\t<src data=\"" << src << "\" />" << std::endl;
		outputfile << "\t<dst data=\"" << dst << "\" />" << std::endl;
		outputfile << "\t<srcchannel data=\"" << srcchannel << "\" />" << std::endl;
		outputfile << "\t<dstchannel data=\"" << dstchannel << "\" />" << std::endl;
		outputfile << "\t<ani data=\"" << ani << "\" />" << std::endl;
		outputfile << "\t<aniii data=\"" << aniii << "\" />" << std::endl;
		outputfile << "\t<network_addr data=\"" << network_addr << "\" />" << std::endl;
		outputfile << "\t<lastapp data=\"" << lastapp << "\" />" << std::endl;
		outputfile << "\t<lastdata data=\"" << lastdata << "\" />" << std::endl;
		outputfile << "\t<billusec data=\"" << billusec << "\" />" << std::endl;
		outputfile << "\t<disposition data=\"" << disposition << "\" />" << std::endl;
		outputfile << "\t<amaflags data=\"" << amaflags << "\" />" << std::endl;
		
		// Now to process chanvars
		outputfile << "\t<chanvars>" << std::endl;
		if(chanvars_supp.size() > 0 )
		{
			std::map<std::string,std::string>::iterator iItr,iEnd;
			for(iItr = chanvars_supp.begin(), iEnd = chanvars_supp.end() ; iItr != iEnd; iItr++)
				outputfile << "\t\t<variable name=\"" << iItr->first << "\" data= \"" << iItr->second << "\" />" << std::endl;
		}
		outputfile << "\t</chanvars>" << std::endl << "</document>" << std::endl << std::endl;

		switch_console_printf(SWITCH_CHANNEL_LOG, "XmlCDR::process_record():  Dumping the CDR to %s.\n",outputfile_name.c_str());
		retval = 1;
	}
	
	return retval;
}

bool XmlCDR::is_activated()
{
	return activated;
}

void XmlCDR::tempdump_record()
{

}

void XmlCDR::reread_tempdumped_records()
{

}

std::string XmlCDR::get_display_name()
{
	return display_name;
}

void XmlCDR::disconnect()
{
	activated = 0;
	connectionstate = 0;
	logchanvars = 0;
	outputfile_path.clear();
	chanvars_supp_list.clear();
	switch_console_printf(SWITCH_CHANNEL_LOG,"Shutting down XmlCDR...  Done!");	
}

AUTO_REGISTER_BASECDR(XmlCDR);

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
