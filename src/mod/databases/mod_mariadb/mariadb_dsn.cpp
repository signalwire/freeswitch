/*
* mod_mariadb for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2019, Andrey Volk <andywolk@gmail.com>
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
* The Original Code is ported from FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* Andrey Volk <andywolk@gmail.com>
*
* mariadb_dsn.cpp -- Connection string parser for MariaDB FreeSWITCH module
*
*/
#include <switch.h>
#include "mariadb_dsn.hpp"

#include <string>
#include <iterator>
#include <vector>
#include <unordered_map>
#include <regex>
#include <algorithm>
#include <sstream>

class mariadb_dsn {

	std::string _host = "localhost";
	std::string _user;
	std::string _passwd;
	std::string _db;
	int _port = 3306;
	std::string _unix_socket;
	std::string _character_set;	
	unsigned long _clientflag;

public:	

	template<typename Out>
	void split(const std::string &s, char delim, Out result) {
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim)) {
			*(result++) = item;
		}
	}

	std::vector<std::string> split(const std::string &s, char delim) {
		std::vector<std::string> elems;
		split(s, delim, std::back_inserter(elems));
		return elems;
	}

	mariadb_dsn(MYSQL *mysql, const char *dsn, unsigned long clientflag)
	{
		_clientflag = clientflag;

		if (dsn) {
			std::vector<std::string> params = split(std::string(dsn), ';');

			for (auto &param : params) {
				std::vector<std::string> pair = split(param, '=');
				if (pair.size() >= 2) {
					std::string key = std::regex_replace(pair[0], std::regex("^ +| +$|( ) +"), "$1");
					std::transform(key.begin(), key.end(), key.begin(), ::tolower);
					std::string value = pair[1];

					if ("server" == key || "host" == key) {
						_host = value;
					} else if ("uid" == key || "user" == key || "username" == key) {
						_user = value;
					} else if ("pwd" == key || "passwd" == key || "password" == key) {
						_passwd = value;
					} else if ("database" == key || "db" == key) {
						_db = value;
					} else if ("port" == key) {
						_port = std::stoi(value);
					} else if ("option" == key || "options" == key) {
						unsigned long option;
						std::stringstream(value) >> option;
						_clientflag |= option;
					} else if ("charset" == key) {
						std::string charset = std::regex_replace(value, std::regex("^ +| +$|( ) +"), "$1");
						int err = mysql_optionsv(mysql, MYSQL_SET_CHARSET_NAME, (void *)charset.c_str());
						if (err) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mysql_optionsv returned an error [MYSQL_SET_CHARSET_NAME=%s]: %s\n", charset.c_str(), mysql_error(mysql));
						}
					}
				}
			}

		}
	}

	const char* host() const
	{
		return _host.c_str();
	}

	const char* user() const
	{
		return _user.c_str();
	}

	const char* passwd() const
	{
		return _passwd.c_str();
	}

	const char* db() const
	{
		return _db.c_str();
	}

	const int port() const
	{
		return _port;
	}

	const char* unix_socket() const
	{
		return ("" == _unix_socket) ? NULL : _unix_socket.c_str();
	}

	unsigned long clientflag()
	{
		return _clientflag;
	}
};

MYSQL* STDCALL mysql_dsn_connect(MYSQL *mysql, const char *connection_string, unsigned long clientflag)
{
	mariadb_dsn dsn(mysql, connection_string, clientflag);
	return mysql_real_connect(mysql, dsn.host(), dsn.user(), dsn.passwd(), dsn.db(), dsn.port(), dsn.unix_socket(), dsn.clientflag());
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
