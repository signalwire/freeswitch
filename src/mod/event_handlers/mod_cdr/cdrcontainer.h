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
 * Description: This C++ header file describes the CDRContainer singleton object used by mod_cdr to control
 * the creation, processing, and destruction of various CDR logger objects.
 *
 * cdrcontainer.h
 *
 */

#ifndef CDRCONTAINER
#define CDRCONTAINER

#ifdef __cplusplus
#include <cstring>
#include <iostream>
#endif
#include "basecdr.h"
#include <switch.h>

/*
The CDRContainer Class will serve as a central receptacle for all CDR methods.  CDRContainer::CDRContainer() will initialize the system, and CDRContainer::add_cdr() will add a new object of each CDR type to a queue for "later processing" by the other thread.
CDRContainer::process_records() will iterate thru the queue and commit the records to the respective backends.
*/

#ifdef __cplusplus
extern "C" {
#endif

class CDRContainer {
	public:
		CDRContainer();
		CDRContainer(switch_memory_pool_t *module_pool);
		~CDRContainer();
		void add_cdr(switch_core_session_t *session);
		void process_records();
#ifdef SWITCH_QUEUE_ENHANCED
		void reload(switch_stream_handle_t *stream);
		void queue_pause(switch_stream_handle_t *stream);
		void queue_resume(switch_stream_handle_t *stream);
#endif
		void active(switch_stream_handle_t *stream);
		void available(switch_stream_handle_t *stream);
	protected:
	private:
		switch_xml_t cfg, xml, settings, param;
		switch_queue_t *cdrqueue;
		std::string tempfilepath;
		char configfile[13];
		bool queue_paused;
};

#ifdef __cplusplus
}
#endif

#endif

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
