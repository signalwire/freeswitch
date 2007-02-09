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
 * Description: BaseRegistry and BaseRegistration classes and AUTO_REGISTER_BASECDR macro provided in part by David 
 * Terrell from his blog posting http://meat.net/2006/03/cpp-runtime-class-registration/  Much thanks to him 
 * for his help.
 *
 * baseregistry.h
 *
 */

#include "basecdr.h"
#include <iostream>

#ifndef BASECDRREGISTRY
#define BASECDRREGISTRY

#ifdef __cplusplus
#include <vector>

template<class T> BaseCDR* basecdr_factory(switch_mod_cdr_newchannel_t *newchannel)
{
	return new T(newchannel);
}

typedef BaseCDR* (*basecdr_creator)(switch_mod_cdr_newchannel_t *newchannel);

class BaseRegistry
{
	private:
		std::vector<basecdr_creator> m_bases; // Stores all modules
		std::vector<basecdr_creator> active_bases; // Stores only active modules
	public:
		typedef std::vector<basecdr_creator>::iterator iterator;
		static BaseRegistry& get();
		void add(basecdr_creator);
		void reset_active(); // Clears the active vector for reloading of configuration.
		void add_active(iterator);
		iterator begin();
		iterator end();
		iterator active_begin();
		iterator active_end();
};

class BaseRegistration
{
	public:
		BaseRegistration(basecdr_creator);
};

#define AUTO_REGISTER_BASECDR(basecdr) BaseRegistration _basecdr_registration_ ## basecdr(&basecdr_factory<basecdr>);

#endif // ifdef __cplusplus
#endif

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
