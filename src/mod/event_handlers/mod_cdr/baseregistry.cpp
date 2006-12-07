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
 * Description: This C++ source describes the BaseRegistry and BaseRegistration classes that implement
 * the factory routine with the aid of the templated base_factory() function.
 *
 * baseregistry.cpp
 *
 */

#include "baseregistry.h"

BaseRegistry& BaseRegistry::get()
{
	static BaseRegistry instance;
	return instance;
}

void BaseRegistry::add(basecdr_creator creator)
{
	m_bases.push_back(creator);
}

BaseRegistry::iterator BaseRegistry::begin()
{
	return m_bases.begin();
}

BaseRegistry::iterator BaseRegistry::end()
{ 
	return m_bases.end(); 
}

void BaseRegistry::reset_active()
{
	active_bases.clear();
}

void BaseRegistry::add_active(iterator tempobject)
{
	active_bases.push_back(*tempobject);
}

BaseRegistry::iterator BaseRegistry::active_begin()
{
	return active_bases.begin();
}

BaseRegistry::iterator BaseRegistry::active_end()
{
	return active_bases.end();
}

BaseRegistration::BaseRegistration(basecdr_creator creator)
{
	BaseRegistry::get().add(creator);
}

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
