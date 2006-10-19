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
 * Ken Rice, Asteria Solutions Group, Inc. <ken AT asteriasgi.com>
 *
 * Description: This C++ header file describes the XmlCDR class which handles formatting a CDR out to
 * individual text files in a XML format.
 *
 * xmlcdr.h
 *
 */

#include "baseregistry.h"
#include <switch.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <list>

#ifndef XMLCDR
#define XMLCDR

class XmlCDR : public BaseCDR {
	public:
		XmlCDR();
		XmlCDR(switch_mod_cdr_newchannel_t *newchannel);
		virtual ~XmlCDR();
		virtual bool process_record();
		virtual void connect(switch_xml_t& cfg, switch_xml_t& xml, switch_xml_t& settings, switch_xml_t& param); // connect and disconnect need to be static because we're persisting connections until shutdown
		virtual void disconnect();
		virtual bool is_activated();
		virtual void tempdump_record();
		virtual void reread_tempdumped_records();

	private:
		static bool activated; // Is this module activated?
		static bool connectionstate; // What is the status of the connection?
		static bool logchanvars;
		static std::string outputfile_path; // The directory we'll dump these into
		static std::list<std::string> chanvars_fixed_list; // Normally this would be used, but not in this class
		static std::list<std::string> chanvars_supp_list; // This will hold the list for all chanvars here
		char formattedcallstartdate[100];
		char formattedcallanswerdate[100];
		char formattedcallenddate[100];
		std::string outputfile_name;
		std::ofstream outputfile;
};

#endif
