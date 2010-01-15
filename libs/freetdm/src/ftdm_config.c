/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "freetdm.h"
#include "ftdm_config.h"

int ftdm_config_open_file(ftdm_config_t *cfg, const char *file_path)
{
	FILE *f;
	const char *path = NULL;
	char path_buf[1024];

	if (file_path[0] == '/') {
		path = file_path;
	} else {
		snprintf(path_buf, sizeof(path_buf), "%s%s%s", FTDM_CONFIG_DIR, FTDM_PATH_SEPARATOR, file_path);
		path = path_buf;
	}

	if (!path) {
		return 0;
	}

	memset(cfg, 0, sizeof(*cfg));
	cfg->lockto = -1;
	ftdm_log(FTDM_LOG_DEBUG, "Configuration file is %s.\n", path);
	f = fopen(path, "r");

	if (!f) {
		if (file_path[0] != '/') {
			int last = -1;
			char *var, *val;

			snprintf(path_buf, sizeof(path_buf), "%s%sfreetdm.conf", FTDM_CONFIG_DIR, FTDM_PATH_SEPARATOR);
			path = path_buf;

			if ((f = fopen(path, "r")) == 0) {
				return 0;
			}

			cfg->file = f;
			ftdm_set_string(cfg->path, path);

			while (ftdm_config_next_pair(cfg, &var, &val)) {
				if ((cfg->sectno != last) && !strcmp(cfg->section, file_path)) {
					cfg->lockto = cfg->sectno;
					return 1;
				}
			}

			ftdm_config_close_file(cfg);
			memset(cfg, 0, sizeof(*cfg));
			return 0;
		}

		return 0;
	} else {
		cfg->file = f;
		ftdm_set_string(cfg->path, path);
		return 1;
	}
}

void ftdm_config_close_file(ftdm_config_t *cfg)
{

	if (cfg->file) {
		fclose(cfg->file);
	}

	memset(cfg, 0, sizeof(*cfg));
}



int ftdm_config_next_pair(ftdm_config_t *cfg, char **var, char **val)
{
	int ret = 0;
	char *p, *end;

	*var = *val = NULL;

	if (!cfg->path) {
		return 0;
	}

	for (;;) {
		cfg->lineno++;

		if (!fgets(cfg->buf, sizeof(cfg->buf), cfg->file)) {
			ret = 0;
			break;
		}
		*var = cfg->buf;

		if (**var == '[' && (end = strchr(*var, ']')) != 0) {
			*end = '\0';
			(*var)++;
			if (**var == '+') {
				(*var)++;
				ftdm_copy_string(cfg->section, *var, sizeof(cfg->section));
				cfg->sectno++;

				if (cfg->lockto > -1 && cfg->sectno != cfg->lockto) {
					break;
				}
				cfg->catno = 0;
				cfg->lineno = 0;
				*var = (char *) "";
				*val = (char *) "";
				return 1;
			} else {
				ftdm_copy_string(cfg->category, *var, sizeof(cfg->category));
				cfg->catno++;
			}
			continue;
		}



		if (**var == '#' || **var == ';' || **var == '\n' || **var == '\r') {
			continue;
		}

		if (!strncmp(*var, "__END__", 7)) {
			break;
		}


		if ((end = strchr(*var, ';')) && *(end+1) == *end) {
			*end = '\0';
			end--;
		} else if ((end = strchr(*var, '\n')) != 0) {
			if (*(end - 1) == '\r') {
				end--;
			}
			*end = '\0';
		}

		p = *var;
		while ((*p == ' ' || *p == '\t') && p != end) {
			*p = '\0';
			p++;
		}
		*var = p;


		if ((*val = strchr(*var, '=')) == 0) {
			ret = -1;
			/* log_printf(0, server.log, "Invalid syntax on %s: line %d\n", cfg->path, cfg->lineno); */
			continue;
		} else {
			p = *val - 1;
			*(*val) = '\0';
			(*val)++;
			if (*(*val) == '>') {
				*(*val) = '\0';
				(*val)++;
			}

			while ((*p == ' ' || *p == '\t') && p != *var) {
				*p = '\0';
				p--;
			}

			p = *val;
			while ((*p == ' ' || *p == '\t') && p != end) {
				*p = '\0';
				p++;
			}
			*val = p;
			ret = 1;
			break;
		}
	}


	return ret;

}

FT_DECLARE (int) ftdm_config_get_cas_bits(char *strvalue, unsigned char *outbits)
{
	char cas_bits[5];
	unsigned char bit = 0x8;
	int x = 0;
	char *double_colon = strchr(strvalue, ':');
	if (!double_colon) {
		ftdm_log(FTDM_LOG_ERROR, "No CAS bits specified: %s, :xxxx definition expected, where x is 1 or 0\n", strvalue);
		return -1;
	}
	double_colon++;
	*outbits = 0;
	cas_bits[4] = 0;
	if (sscanf(double_colon, "%c%c%c%c", &cas_bits[0], &cas_bits[1], &cas_bits[2], &cas_bits[3]) != 4) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid CAS bits specified: '%s', :xxxx definition expected, where x is 1 or 0\n", double_colon);
		return -1;
	}
	ftdm_log(FTDM_LOG_DEBUG, "CAS bits specification found: %s\n", cas_bits);
	for (; cas_bits[x]; x++) {
		if ('1' == cas_bits[x]) {
			*outbits |= bit;
		} else if ('0' != cas_bits[x]) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid CAS pattern specified: %s, just 0 or 1 allowed for each bit\n");
			return -1;
		}
		bit >>= 1;
	}
	return 0;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
