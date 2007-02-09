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
 * Description: This C++ source file describes the BaseCDR class that all other CDR classes inherit from.
 * It handles the bulk of the processing of data from the switch_channel_t objects.
 *
 * basecdr.cpp
 *
 */

#include "basecdr.h"
#include <iostream>
#include <string>
#include <cstring>
#include <switch.h>

BaseCDR::BaseCDR()
{
	callstartdate = 0;
	callanswerdate = 0;
	callenddate = 0;
	strncpy(clid,"Unknown",7);
	strncpy(src,"-1",2);
	strncpy(dst,"-1",2);
	strncpy(srcchannel,"-1",2);
	strncpy(dstchannel,"-1",2);
	strncpy(lastapp,"-1",2);
	billusec = 0;
	disposition=0;
	amaflags = 0;
	errorstate = 0;
}

BaseCDR::BaseCDR(switch_mod_cdr_newchannel_t *newchannel)
{
	if(newchannel != 0)
	{
		errorstate = 0;
		originated=1; // One-legged calls are always considered the originator
		memset(clid,0,80);
		memset(dialplan,0,80);
		memset(myuuid,0,37);
		memset(destuuid,0,37);
		memset(dialplan,0,80);
		memset(&hangupcause,0,sizeof(hangupcause));
		hangupcause_text = 0;
		memset(src,0,80);
		memset(dst,0,80);
		memset(srcchannel,0,80);
		memset(dstchannel,0,80);
		memset(network_addr,0,40);
		memset(ani,0,80);
		memset(aniii,0,80);
		memset(lastapp,0,80);
		memset(lastdata,0,255);
		
		// switch_channel_timetable_t *timetable = switch_channel_get_timetable(newchannel->channel->caller_profile);
		
		coresession = newchannel->session;
	
		if (newchannel->callerprofile)
		{
			callstartdate= newchannel->callerprofile->times->created;
			callanswerdate = newchannel->callerprofile->times->answered;
			calltransferdate = newchannel->callerprofile->times->transferred;
			callenddate = newchannel->callerprofile->times->hungup;

			if(newchannel->callerprofile->caller_id_name != 0)
			{
				strncpy(clid,newchannel->callerprofile->caller_id_name,strlen(newchannel->callerprofile->caller_id_name));
				strncat(clid," <",2);
				if(newchannel->callerprofile->caller_id_number != 0 )
					strncat(clid,newchannel->callerprofile->caller_id_number,strlen(clid)+strlen(newchannel->callerprofile->caller_id_number));
				strncat(clid,">",1);
			}
			
			// Get the ANI information if it's set
			if(newchannel->callerprofile->ani != 0)
				strncpy(ani,newchannel->callerprofile->ani,strlen(newchannel->callerprofile->ani));
			if(newchannel->callerprofile->aniii != 0)
				strncpy(aniii,newchannel->callerprofile->aniii,strlen(newchannel->callerprofile->aniii));
		
			if(newchannel->callerprofile->dialplan != 0)
				strncpy(dialplan,newchannel->callerprofile->dialplan,strlen(newchannel->callerprofile->dialplan));
		
			if(newchannel->callerprofile->network_addr != 0)
				strncpy(network_addr,newchannel->callerprofile->network_addr,strlen(newchannel->callerprofile->network_addr));
		}
		
		//switch_caller_profile_t *originateprofile = switch_channel_get_originator_caller_profile(newchannel->channel->callerprofile);
		
		// Were we the receiver of the call?
		if(newchannel->callerprofile->originator_caller_profile)
		{
			originated = 0;
			if(newchannel->callerprofile->originator_caller_profile->uuid != 0)
				strncpy(destuuid,newchannel->callerprofile->originator_caller_profile->uuid,strlen(newchannel->callerprofile->originator_caller_profile->uuid));
			if(newchannel->callerprofile)
			{
				if(newchannel->callerprofile->destination_number)
					strncpy(src,newchannel->callerprofile->destination_number,strlen(newchannel->callerprofile->destination_number));
				if(newchannel->callerprofile->caller_id_number != 0)
					strncpy(dst,newchannel->callerprofile->caller_id_number,strlen(newchannel->callerprofile->caller_id_number));
			}
		}
		else
		{
			//originateprofile = switch_channel_get_originatee_profile(newchannel->channel->callerprofile);
			// Or were we maybe we were the caller?
			if(newchannel->callerprofile->originatee_caller_profile)
			{
				if (newchannel->callerprofile) {
					if(newchannel->callerprofile->caller_id_number != 0)
						strncpy(src,newchannel->callerprofile->caller_id_number,strlen(newchannel->callerprofile->caller_id_number));
					if(newchannel->callerprofile->destination_number != 0)
						strncpy(dst,newchannel->callerprofile->destination_number,strlen(newchannel->callerprofile->destination_number));
				}
				if(newchannel->callerprofile->originatee_caller_profile->chan_name != 0)
					strncpy(dstchannel,newchannel->callerprofile->originatee_caller_profile->chan_name,strlen(newchannel->callerprofile->originatee_caller_profile->chan_name));
			}
		}
		
		strncpy(myuuid,newchannel->callerprofile->uuid,strlen(newchannel->callerprofile->uuid));
		strncpy(srcchannel,newchannel->callerprofile->chan_name,strlen(newchannel->callerprofile->chan_name));
		
		if(switch_channel_test_flag(newchannel->channel,CF_ANSWERED))
		{
			disposition=1;
			if(callstartdate)
				billusec = callenddate - callanswerdate;
			else
				billusec = callenddate - calltransferdate;
		}
		else if(switch_channel_test_flag(newchannel->channel,CF_TRANSFER))
		{
			disposition=1;
			billusec = callenddate - calltransferdate;
		}
		else
		{
			disposition=0;
			billusec = 0;
		}
		
		// What was the hangup cause?
		hangupcause = switch_channel_get_cause(newchannel->channel);
		hangupcause_text = switch_channel_cause2str(hangupcause);
	
		if(newchannel->callerextension != 0)
			if(newchannel->callerextension->last_application != 0)
			{
				if(newchannel->callerextension->last_application->application_name != 0)
					strncpy(lastapp,newchannel->callerextension->last_application->application_name,strlen(newchannel->callerextension->last_application->application_name));
				if(newchannel->callerextension->last_application->application_data != 0)
					strncpy(lastdata,newchannel->callerextension->last_application->application_data,strlen(newchannel->callerextension->last_application->application_data));
			}
		
		amaflags=0;
	}
}

BaseCDR::~BaseCDR()
{

}

/* bool fixed is for checking if this is the fixed or supplemental list */
void BaseCDR::parse_channel_variables_xconfig(std::string& unparsed,std::list<std::string>& chanvarlist,bool fixed)
{
	std::string tempstring;
	for(std::string::size_type i = 0, j = 0; j != std::string::npos; )
	{
		j = unparsed.find(',',i);
		if(j > 0)
		{
			tempstring = unparsed.substr(i,(j-i));
			chanvarlist.push_back(tempstring);
			i =j+1;
		}
		else
		{
			tempstring = unparsed.substr(i);
			chanvarlist.push_back(tempstring);
		}
	}
	
	// Now we need to clean up in case somebody put in a '*' in the chanvars fixed list
	std::list<std::string>::iterator iBeg,iItr,iEnd;
	iBeg = chanvarlist.begin();
	iEnd = chanvarlist.end();
	bool exitcode = 1;
	for(iItr = chanvarlist.begin(); iItr != iEnd && exitcode ; )
	{
		if(iItr->find('*',0) != std::string::npos)
		{
			if(fixed)
			{
				switch_console_printf(SWITCH_CHANNEL_LOG,"Wildcards are not allow in the fixed chanvars list.  Item removed.\n");
				iItr = chanvarlist.erase(iItr);
			}
			else
			{
				if(chanvarlist.size() > 1) 
				chanvarlist.clear();
				std::string all = "*";
				chanvarlist.push_back(all);
				exitcode = 0;
			}
		}
		else
			iItr++;
	}
}

void BaseCDR::parse_channel_variables_xconfig(std::string& unparsed,std::list<std::string>& chanvarlist,std::vector<switch_mod_cdr_sql_types_t>& chanvars_fixed_types)
{
	bool fixed = 1;
	std::string tempstring, tempstring2;
	switch_mod_cdr_sql_types_t sql_type;
	parse_channel_variables_xconfig(unparsed,chanvarlist,fixed);
	std::list<std::string> tempchanvarlist; // = chanvarlist;
	std::list<std::string>::iterator iEnd, iItr;
	
	for(iItr = chanvarlist.begin(), iEnd = chanvarlist.end() ; iItr != iEnd ; iItr++)
	{
		sql_type = CDR_STRING;
		std::string::size_type i = 0, j = 0;
		j = iItr->find('=',i);
		if(j > 0 )
		{
			tempstring = iItr->substr(i,(j-i));
			i =j+1;
			tempstring2 = iItr->substr(i);
			if(tempstring2.size() == 1)
			{
				switch(tempstring2[0])
				{
					case 'I':
					case 'i':
						sql_type = CDR_INTEGER;
						break;
					case 'S':
					case 's':
						sql_type = CDR_STRING;
						break;
					case 'D':
					case 'd':
						sql_type = CDR_DOUBLE;
						break;
					case 'T':
					case 't':
						sql_type = CDR_TINY;
						break;
					default:
						switch_console_printf(SWITCH_CHANNEL_LOG,"Valid fixed channel variable types are x (decimal), d (double), i (integer), t (tiny), s (string).  You tried to give a type of %s to chanvar %s.\nReverting this chanvar type to a string type.\n",tempstring2.c_str(),tempstring.c_str());
						sql_type = CDR_STRING;
				}
			}
			else
			{
				switch_console_printf(SWITCH_CHANNEL_LOG,"Valid fixed channel variable types are x (decimal), d (double), i (integer), t (tiny), s (string).  You tried to give a type of %s to chanvar %s.\nReverting this chanvar type to a string type.\n",tempstring2.c_str(),tempstring.c_str());
				sql_type = CDR_STRING;
			}
		}
		else
		{
			switch_console_printf(SWITCH_CHANNEL_LOG,"No parameter set, for channel variable %s, using default type of string.\n",iItr->c_str());
			sql_type = CDR_STRING;
			tempstring = *iItr;
		}
			
		tempchanvarlist.push_back(tempstring);
		chanvars_fixed_types.push_back(sql_type);
	}
	
	chanvarlist.clear();
	chanvarlist = tempchanvarlist;
}

// This is for processing the fixed channel variables
void BaseCDR::process_channel_variables(const std::list<std::string>& stringlist,switch_channel_t *channel)
{
	std::list<std::string>::const_iterator iItr,iEnd;
	iEnd = stringlist.end();
		
	for(iItr = stringlist.begin(); iItr != iEnd; iItr++)
	{
		std::vector<char> tempstringvector(iItr->begin(), iItr->end());
		tempstringvector.push_back('\0');
		char* tempstring= &tempstringvector[0];

		char *tempvariable;
		tempvariable = switch_channel_get_variable(channel,tempstring);
		
		
		std::pair<std::string,std::string> temppair;
		temppair.first = *iItr;
			
		if(tempvariable != 0)
			temppair.second = tempvariable;
		
		chanvars_fixed.push_back(temppair);
	}
}

// This one is for processing of supplemental chanvars
void BaseCDR::process_channel_variables(const std::list<std::string>& stringlist, const std::list<std::string>& fixedlist, switch_channel_t *channel,bool repeat)
{
	if(stringlist.front() == "*")
	{
		switch_hash_index_t *hi;
		void *val;
		const void *var;
		switch_memory_pool_t *sessionpool;
		sessionpool = switch_core_session_get_pool(coresession);
		for (hi = switch_channel_variable_first(channel,sessionpool); hi; hi = switch_hash_next(hi)) 
		{
			switch_hash_this(hi, &var, 0, &val);
			std::string tempstring_first, tempstring_second;
			tempstring_first = (char *) var;
			tempstring_second = (char *) val;
			chanvars_supp[tempstring_first] = tempstring_second;
		}
	}
	else
	{
		std::list<std::string>::const_iterator iItr,iEnd;
		iEnd = stringlist.end();
			
		for(iItr = stringlist.begin(); iItr != iEnd; iItr++)
		{
			std::vector<char> tempstringvector(iItr->begin(), iItr->end());
			tempstringvector.push_back('\0');
			char* tempstring= &tempstringvector[0];

			char *tempvariable;
			tempvariable = switch_channel_get_variable(channel,tempstring);
			if(tempvariable != 0)
				chanvars_supp[*iItr] = tempvariable;
		}
	}
	
	if(!repeat)
	{
		std::map<std::string,std::string>::iterator MapItr;
		std::list<std::string>::const_iterator iItr,iEnd;
		for(iItr = fixedlist.begin(), iEnd = fixedlist.end(); iItr != iEnd; iItr++)
		{
			MapItr = chanvars_supp.find(*iItr);
			if(MapItr != chanvars_supp.end() )
				chanvars_supp.erase(MapItr);
		}
	}
}

void BaseCDR::escape_string(std::string& src)
{
	std::string::size_type pos = 0;
	
	// find all occurences of ' and replace them with \'
	while ( ( pos = src.find( "'", pos ) ) != std::string::npos ) 
	{
		src.replace( pos, 1, "\\'" );
		pos += 2;
	}
}

std::string BaseCDR::escape_chararray(char* src)
{
	std::string src_string = src;
	
	std::string::size_type pos = 0;
	
	// find all occurences of ' and replace them with \'
	while ( ( pos = src_string.find( "'", pos ) ) != std::string::npos ) 
	{
		src_string.replace( pos, 1, "\\'" );
		pos += 2;
	}
	
	return src_string;
}


/* For Emacs:
 * Local Variables:
 * mode:c++
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
