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
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
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
	
	queue_paused = 0;
	
	strcpy(configfile,"cdr.conf");
	
	switch_mod_cdr_newchannel_t *newchannel; // = new switch_mod_cdr_newchannel_t;
	newchannel = 0;
	
	if (!(xml = switch_xml_open_cfg(configfile, &cfg, NULL))) 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", configfile);
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
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_cdr shutdown gracefully.");
}

#ifdef SWITCH_QUEUE_ENHANCED
void CDRContainer::reload(switch_stream_handle_t *stream)
{
	// The queue can't be paused otherwise it will never be able to reload safely.
	if(queue_paused)
	{
		stream->write_function(stream,"The queue is currently paused, resuming it.\n");
		queue_resume(stream);
	}
	// Something tells me I still need to figure out what to do if there are items still in queue after reload that are no longer active in the configuration.
	switch_queue_isempty(cdrqueue); // Waits for the queue to be empty
	
	switch_mod_cdr_newchannel_t *newchannel; // = new switch_mod_cdr_newchannel_t;
	newchannel = 0;
	
	const char *err;
	switch_xml_t xml_root;
	
	if ((xml_root = switch_xml_open_root(1, &err))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reloading the XML file...\n");
		switch_xml_free(xml_root);
	}
	
	if (!(xml = switch_xml_open_cfg(configfile, &cfg, NULL))) 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "open of %s failed\n", configfile);
	else
	{
		BaseRegistry& registry(BaseRegistry::get());
		for(BaseRegistry::iterator it = registry.active_begin(); it != registry.active_end(); ++it)
		{
			basecdr_creator func = *it;
			BaseCDR* _ptr = func(newchannel);
			std::auto_ptr<BaseCDR> ptr(_ptr);
			ptr->disconnect();
		}
		
		registry.reset_active();
		
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
	switch_queue_unblockpop(cdrqueue);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_cdr configuration reloaded.");
}

void CDRContainer::queue_pause(switch_stream_handle_t *stream)
{
	if(queue_paused)
		stream->write_function(stream,"Queue is already paused.\n");
	else
	{
		queue_paused = 1;
		switch_queue_blockpop(cdrqueue);
		stream->write_function(stream,"CDR queue is now paused.  Beware that this can waste resources the longer you keep it paused.\n");
	}
}

void CDRContainer::queue_resume(switch_stream_handle_t *stream)
{
	if(!queue_paused)
		stream->write_function(stream,"Queue is currently running, no need to resume it.\n");
	else
	{
		queue_paused = 0;
		switch_queue_unblockpop(cdrqueue);
		stream->write_function(stream,"CDR queue has now resumed processing CDR records.\n");
	}

}
#endif

void CDRContainer::active(switch_stream_handle_t *stream)
{
	switch_mod_cdr_newchannel_t *newchannel; // = new switch_mod_cdr_newchannel_t;
	newchannel = 0;
	
	stream->write_function(stream,"The following mod_cdr logging backends are currently marked as active:\n");
	BaseRegistry& registry(BaseRegistry::get());
	for(BaseRegistry::iterator it = registry.active_begin(); it != registry.active_end(); ++it)
	{
		basecdr_creator func = *it;
		BaseCDR* _ptr = func(newchannel);
		std::auto_ptr<BaseCDR> ptr(_ptr);
		stream->write_function(stream,"%s\n",ptr->get_display_name().c_str());
	}
}

void CDRContainer::available(switch_stream_handle_t *stream)
{
	switch_mod_cdr_newchannel_t *newchannel; // = new switch_mod_cdr_newchannel_t;
	newchannel = 0;
	
	stream->write_function(stream,"The following mod_cdr logging backends are currently avaible for use (providing you configure them):\n");
	BaseRegistry& registry(BaseRegistry::get());
	for(BaseRegistry::iterator it = registry.begin(); it != registry.end(); ++it)
	{
		basecdr_creator func = *it;
		BaseCDR* _ptr = func(newchannel);
		std::auto_ptr<BaseCDR> ptr(_ptr);
		stream->write_function(stream,"%s\n",ptr->get_display_name().c_str());
	}
}

void CDRContainer::add_cdr(switch_core_session_t *session)
{
	switch_mod_cdr_newchannel_t *newchannel = new switch_mod_cdr_newchannel_t;
	memset(newchannel,0,sizeof(*newchannel));
	
	newchannel->channel = switch_core_session_get_channel(session);
	assert(newchannel->channel != 0);

	newchannel->session = session;
	newchannel->callerextension = switch_channel_get_caller_extension(newchannel->channel);
	newchannel->callerprofile = switch_channel_get_caller_profile(newchannel->channel);
	
	while (newchannel->callerprofile)
	{
		BaseRegistry& registry(BaseRegistry::get());
		for(BaseRegistry::iterator it = registry.active_begin(); it != registry.active_end(); ++it)
		{
			basecdr_creator func = *it;
			
			BaseCDR* newloggerobject = func(newchannel);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Adding a new logger object to the queue.\n");
			switch_queue_push(cdrqueue,newloggerobject);
		}
		newchannel->callerprofile = newchannel->callerprofile->next;
		if(newchannel->callerextension)
			newchannel->callerextension = newchannel->callerextension->next;
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
