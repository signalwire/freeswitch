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
 * Description: This C++ source file describes the CDRContainer singleton object used by mod_cdr to control
 * the creation, processing, and destruction of various CDR logger objects.
 *
 * cdrcontainer.cpp
 *
 */

#include "cdrcontainer.h"
#include "baseregistry.h"

CDRContainer::CDRContainer()
{

}

CDRContainer::CDRContainer(switch_memory_pool_t *module_pool)
{
	// Create the APR threadsafe queue, though I don't know if this is the current memory pool.
	switch_queue_create(&cdrqueue,5224288, module_pool);
	
	char *configfile = "mod_cdr.conf";
	switch_xml_t cfg, xml, settings, param;
	
	switch_mod_cdr_newchannel_t *newchannel; // = new switch_mod_cdr_newchannel_t;
	newchannel = 0;
	
	if (!(xml = switch_xml_open_cfg(configfile, &cfg, NULL))) 
		switch_console_printf(SWITCH_CHANNEL_LOG,"open of %s failed\n", configfile);
	else
	{	
		BaseRegistry& registry(BaseRegistry::get());
		for(BaseRegistry::iterator it = registry.begin(); it != registry.end(); ++it)
		{
			basecdr_creator func = *it;
			BaseCDR* _ptr = func(newchannel);
			std::auto_ptr<BaseCDR> ptr(_ptr);
			ptr->connect(cfg,xml,settings,param);
			
			if(ptr->is_activated())
				registry.add_active(it);
		}
	}

	switch_xml_free(xml);
}

CDRContainer::~CDRContainer()
{
	if(switch_queue_size(cdrqueue) > 0)
		process_records();
	
	switch_mod_cdr_newchannel_t *newchannel; //= new switch_mod_cdr_newchannel_t;
	newchannel = 0;
	
	BaseRegistry& registry(BaseRegistry::get());
	for(BaseRegistry::iterator it = registry.active_begin(); it != registry.active_end(); ++it)
	{
		basecdr_creator func = *it;
		BaseCDR* _ptr = func(newchannel);
		std::auto_ptr<BaseCDR> ptr(_ptr);
		ptr->disconnect();
	}
		
	switch_console_printf(SWITCH_CHANNEL_LOG,"mod_cdr shutdown gracefully.");
}

void CDRContainer::add_cdr(switch_core_session_t *session)
{
	switch_mod_cdr_newchannel_t *newchannel = new switch_mod_cdr_newchannel_t;
	memset(newchannel,0,sizeof(*newchannel));
	
	newchannel->channel = switch_core_session_get_channel(session);
	assert(newchannel->channel != 0);

	newchannel->session = session;
	newchannel->timetable = switch_channel_get_timetable(newchannel->channel);
	newchannel->callerextension = switch_channel_get_caller_extension(newchannel->channel);
	newchannel->callerprofile = switch_channel_get_caller_profile(newchannel->channel);
	newchannel->originateprofile = switch_channel_get_originator_caller_profile(newchannel->channel);	
	
	BaseRegistry& registry(BaseRegistry::get());
	for(BaseRegistry::iterator it = registry.active_begin(); it != registry.active_end(); ++it)
	{
		/* 
		   First time it might be originator profile, or originatee.  Second and 
		   after is always going to be originatee profile.
		*/
		
		basecdr_creator func = *it;
		
		if(newchannel->originateprofile != 0 )
		{
			BaseCDR* newloggerobject = func(newchannel);
			switch_console_printf(SWITCH_CHANNEL_LOG,"Adding a new logger object to the queue.\n");
			switch_queue_push(cdrqueue,newloggerobject);
			
			if(newchannel->timetable->next != 0)
			{
				newchannel->originateprofile = switch_channel_get_originatee_caller_profile(newchannel->channel);
				newchannel->originate = 1;
			}	
		}
		else
		{
			newchannel->originateprofile = switch_channel_get_originatee_caller_profile(newchannel->channel);
			newchannel->originate = 1;
			
			BaseCDR* newloggerobject = func(newchannel);
			switch_console_printf(SWITCH_CHANNEL_LOG,"Adding a new logger object to the queue.\n");
			switch_queue_push(cdrqueue,newloggerobject);
		}
		
		while (newchannel->timetable->next != 0 && newchannel->callerextension->next != 0 && newchannel->callerprofile->next != 0 && newchannel->originateprofile->next != 0 ) 
		{			
			newchannel->timetable = newchannel->timetable->next;
			newchannel->callerprofile = newchannel->callerprofile->next;
			newchannel->callerextension = newchannel->callerextension->next;
			newchannel->originateprofile = newchannel->originateprofile->next;
			
			BaseCDR* newloggerobject = func(newchannel);
			switch_console_printf(SWITCH_CHANNEL_LOG,"Adding a new logger object to the queue.\n");
			switch_queue_push(cdrqueue,newloggerobject);
		}
	}
	
	delete newchannel;
}

void CDRContainer::process_records()
{
		BaseCDR *tempnewloggerobject = 0, *newloggerobject = 0;
		while(switch_queue_pop(cdrqueue,reinterpret_cast< void** > (&tempnewloggerobject))== SWITCH_STATUS_SUCCESS)
		{
			newloggerobject = tempnewloggerobject;
			newloggerobject->process_record();
			delete newloggerobject;
		}
}
