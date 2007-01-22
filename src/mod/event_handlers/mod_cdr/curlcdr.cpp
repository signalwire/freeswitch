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
 * This code is largely derived from csvcdr.cpp with minor code snippets from mod_xml_curl.c and edited by 
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * The Initial Developer of the Original Code is
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 * Description: This C++ source file describes the CurlCDR class that handles processing CDRs to HTTP endpoint.
 * This is the standard Curl module, and has a list of predefined variables to log out which can be
 * added to, but never have the default ones removed.  If you want to use one that allows you to explicity
 * set all data variables to be logged and in what order, then this is not the class you want to use, and
 * one will be coming in the future to do just that.
 *
 * curlcdr.cpp
 *
 */

#include <switch.h>
#include "curlcdr.h"
#include <string>
#include <curl/curl.h>


CurlCDR::CurlCDR() : BaseCDR()
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcallenddate,0,100);
}

CurlCDR::CurlCDR(switch_mod_cdr_newchannel_t *newchannel) : BaseCDR(newchannel)
{
	memset(formattedcallstartdate,0,100);
	memset(formattedcallanswerdate,0,100);
	memset(formattedcalltransferdate,0,100);
	memset(formattedcallenddate,0,100);
	
	if(newchannel != 0)
	{
		switch_time_exp_t tempcallstart, tempcallanswer, tempcalltransfer, tempcallend;
		memset(&tempcallstart,0,sizeof(tempcallstart));
		memset(&tempcalltransfer,0,sizeof(tempcalltransfer));
		memset(&tempcallanswer,0,sizeof(tempcallanswer));
		memset(&tempcallend,0,sizeof(tempcallend));
		convert_time(&tempcallstart, callstartdate);
		convert_time(&tempcallanswer, callanswerdate);
		convert_time(&tempcalltransfer, calltransferdate);
		convert_time(&tempcallend, callenddate);
		
		// Format the times
		apr_size_t retsizecsd, retsizecad, retsizectd, retsizeced;  //csd == callstartdate, cad == callanswerdate, ced == callenddate, ceff == callenddate_forfile
		char format[] = "%Y-%m-%d %H:%M:%S";
		switch_strftime(formattedcallstartdate,&retsizecsd,sizeof(formattedcallstartdate),format,&tempcallstart);
		switch_strftime(formattedcallanswerdate,&retsizecad,sizeof(formattedcallanswerdate),format,&tempcallanswer);
		switch_strftime(formattedcalltransferdate,&retsizectd,sizeof(formattedcalltransferdate),format,&tempcalltransfer);
		switch_strftime(formattedcallenddate,&retsizeced,sizeof(formattedcallenddate),format,&tempcallend);

		process_channel_variables(chanvars_fixed_list,newchannel->channel);
		process_channel_variables(chanvars_supp_list,chanvars_fixed_list,newchannel->channel,0);
	}
}

CurlCDR::~CurlCDR()
{

}

bool CurlCDR::activated=0;
bool CurlCDR::logchanvars=0;
bool CurlCDR::connectionstate=0;
modcdr_time_convert_t CurlCDR::convert_time = switch_time_exp_lt;
const char *CurlCDR::gateway_url;
const char *CurlCDR::gateway_credentials;
std::list<std::string> CurlCDR::chanvars_fixed_list;
std::list<std::string> CurlCDR::chanvars_supp_list;
std::string CurlCDR::display_name = "CurlCDR - The HTTP CDR logger";
std::string CurlCDR::postdata;


void CurlCDR::connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param)
{
	switch_console_printf(SWITCH_CHANNEL_LOG, "CurlCDR::connect() - Loading configuration file.\n");
	activated = 0; // Set it as inactive initially
	connectionstate = 0; // Initialize it to false to show that we aren't yet connected.

    switch_console_printf(SWITCH_CHANNEL_LOG,"Checking to see if curlcdr is valid\n");
	if ((settings = switch_xml_child(cfg, "curlcdr"))) 
	{
        switch_console_printf(SWITCH_CHANNEL_LOG,"curlcdr appears to be!!!\n");
		int count_config_params = 0;  // Need to make sure all params are set before we load
		for (param = switch_xml_child(settings, "param"); param; param = param->next) 
		{
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "gateway_url"))
			{
				if(val != 0)
					gateway_url = val;
				count_config_params++;
			}
            else if (!strcmp(var, "gateway_credentials"))
            {
                if(val != 0)
                    gateway_credentials=val;
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
		{
            if(strlen(gateway_url))
			{
				activated = 1;
				switch_console_printf(SWITCH_CHANNEL_LOG,"CurlCDR activated");
			}
            else
                switch_console_printf(SWITCH_CHANNEL_LOG,"CurlCDR::connect(): You must specify a gateway_url to have the records logged to.\n");
		}
		else
			switch_console_printf(SWITCH_CHANNEL_LOG,"CurlCDR::connect(): You did not specify the minimum parameters for using this module.  You must specify at least a gateway_url to have the records logged to.\n");
	}
}



// from Bjarne Stroustrup's page (the guy who designed and implemented C++)
// surely his code has to be better than mine - ok he did say this was the
// *easiest* way and made no comment on it being best :)
std::string CurlCDR::itos(int i)
{
    std::stringstream s;
    s << i;
    return s.str();
}

std::string CurlCDR::lltos(long long ll)
{
    std::stringstream s;
    s << ll;
    return s.str();
}


bool CurlCDR::process_record()
{
    CURL *curl_handle = NULL;
    static char curl_errorstr[CURL_ERROR_SIZE];
	bool retval = 0;
    char *curlescaped;


    curl_handle = curl_easy_init();
    if(!strncasecmp(gateway_url,"https",5)) {
        curl_easy_setopt(curl_handle,CURLOPT_SSL_VERIFYPEER,0);
        curl_easy_setopt(curl_handle,CURLOPT_SSL_VERIFYHOST,0);
    }

    // build the HTTP POST data block
    // WARNING - We need to make sure that the data added is properly escaped, else some sneaky person could set
    //           their CLID to "My Name&billusec=0" for example, which would result in a post of 
    //           clid=My+Name&billusec=0
    //           Simply calculating the billusec in the web app wont solve this because they could play the same
    //           game with callstarttime and all of that.  
    //
    //           The solution I came up with was to use curl_easy_escape() which does a strlen() for every value, 
    //           as well as a malloc of some sort, and thus requires a curl_free().  This is *not* efficient, if 
    //           anyone has a better idea, that would be great.  We cant rely on this after the string is assembled
    //           however because things like the real & would be escaped and that would break things.
    //           
    //           To compensate I only do this where its likely to be a problem.  Preformatted dates are not likely
    //           to ever have such data in them.
    postdata.append("callstartdate=" + lltos(callstartdate));
    postdata.append("&formattedcallstartdate=");
    postdata.append(formattedcallstartdate);
    postdata.append("&callanswerdate=" + lltos(callanswerdate));
    postdata.append("&formattedcallanswerdate=");
    postdata.append(formattedcallanswerdate);
    postdata.append("&calltransferdate=" + lltos(calltransferdate));
    postdata.append("&formattedcalltransferdate=");
    postdata.append(formattedcalltransferdate);
    postdata.append("&callenddate=" + lltos(callenddate));
    postdata.append("&formattedcallenddate=");
    postdata.append(formattedcallenddate);
    postdata.append("&hangupcause_text=");
    postdata.append(hangupcause_text);
    postdata.append("&hangupcause=" + itos(hangupcause));
    postdata.append("&clid=");
    curlescaped = curl_easy_escape(curl_handle,clid,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append(originated==FALSE?"&originated=0":"&originated=1");
    postdata.append("&dialplan=");
    curlescaped = curl_easy_escape(curl_handle,dialplan,0); // admin may have used 'bad chars' in the dialplan name
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append("&myuuid=");
    postdata.append(myuuid);
    postdata.append("&destuuid=");
    postdata.append(destuuid);
    postdata.append("&src=");
    curlescaped = curl_easy_escape(curl_handle,src,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append("&dst=");
    curlescaped = curl_easy_escape(curl_handle,dst,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append("&srcchannel=");
    curlescaped = curl_easy_escape(curl_handle,srcchannel,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append("&dstchannel=");
    curlescaped = curl_easy_escape(curl_handle,dstchannel,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append("&ani=");
    postdata.append(ani); // this may need to be escaped but is likely to be safe since it should be numeric
    postdata.append("&aniii=");
    postdata.append(aniii); // this has to be numeric to be valid
    postdata.append("&network_addr=");
    postdata.append(network_addr);
    postdata.append("&lastapp=");
    curlescaped = curl_easy_escape(curl_handle,lastapp,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    postdata.append("&lastdata=");
    curlescaped = curl_easy_escape(curl_handle,lastdata,0);
    postdata.append(curlescaped);
    curl_free(curlescaped);
    
    postdata.append("&billusec=" + lltos(billusec));
    postdata.append("&disposition=" + itos(disposition));
    postdata.append("&amaflags=" + itos(amaflags));

    // Now to process chanvars, fixed ones first
    if(chanvars_fixed.size() > 0 ) {
        std::list<std::pair<std::string,std::string> >::iterator iItr, iEnd;
        for(iItr = chanvars_fixed.begin(), iEnd = chanvars_fixed.end(); iItr != iEnd; iItr++) {
            curlescaped = curl_easy_escape(curl_handle,iItr->second.c_str(),0);
            if(curlescaped != (char *)NULL) {
                postdata.append("&" + iItr->first + "=" + curlescaped);
                curl_free(curlescaped);
            }
        }
    }
		
    if(chanvars_supp.size() > 0 ) {
        std::map<std::string,std::string>::iterator iItr,iEnd;
        for(iItr = chanvars_supp.begin(), iEnd = chanvars_supp.end() ; iItr != iEnd; iItr++) {
            curlescaped = curl_easy_escape(curl_handle,iItr->second.c_str(),0);
            if(curlescaped != (char *)NULL) {
                postdata.append("&" + iItr->first + "=");
                postdata.append(curlescaped);
                curl_free(curlescaped);
            }
        }
    }

        
    if(!switch_strlen_zero(gateway_credentials)) {
        curl_easy_setopt(curl_handle,CURLOPT_HTTPAUTH,CURLAUTH_ANY);
        curl_easy_setopt(curl_handle,CURLOPT_USERPWD,gateway_credentials);
    }
    curl_easy_setopt(curl_handle,CURLOPT_POST,1);
    curl_easy_setopt(curl_handle,CURLOPT_POSTFIELDS,postdata.c_str());
    curl_easy_setopt(curl_handle,CURLOPT_URL,gateway_url);
    curl_easy_setopt(curl_handle,CURLOPT_USERAGENT,"freeswitch-xml/1.0");
    curl_easy_setopt(curl_handle,CURLOPT_ERRORBUFFER,curl_errorstr);
    if(curl_easy_perform(curl_handle)) {
        switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR,"CurlCDR::process_record() - Error logging CDR record - %s\n",curl_errorstr);
    }
    curl_easy_cleanup(curl_handle);

	retval = 1;
	return retval;
}

bool CurlCDR::is_activated()
{
	return activated;
}

void CurlCDR::tempdump_record()
{

}

void CurlCDR::reread_tempdumped_records()
{

}

std::string CurlCDR::get_display_name()
{
	return display_name;
}

void CurlCDR::disconnect()
{
	activated = 0;
	logchanvars = 0;
	chanvars_fixed_list.clear();
	chanvars_supp_list.clear();
	connectionstate = 0;
	switch_console_printf(SWITCH_CHANNEL_LOG,"Shutting down CurlCDR...  Done!");	
}

AUTO_REGISTER_BASECDR(CurlCDR);

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
