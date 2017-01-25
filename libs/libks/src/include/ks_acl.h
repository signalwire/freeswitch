/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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


#ifndef _KS_ACL_H_
#define _KS_ACL_H_

KS_BEGIN_EXTERN_C

typedef union{
	uint32_t v4;
	struct in6_addr v6;
} ks_ip_t;


#define switch_network_list_validate_ip(_list, _ip) switch_network_list_validate_ip_token(_list, _ip, NULL);

#define switch_test_subnet(_ip, _net, _mask) (_mask ? ((_net & _mask) == (_ip & _mask)) : _net ? _net == _ip : 1)


KS_DECLARE(int) ks_parse_cidr(const char *string, ks_ip_t *ip, ks_ip_t *mask, uint32_t *bitp);
KS_DECLARE(ks_status_t) ks_network_list_create(ks_network_list_t **list, const char *name, ks_bool_t default_type,
														   ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_network_list_add_cidr_token(ks_network_list_t *list, const char *cidr_str, ks_bool_t ok, const char *token);
#define ks_network_list_add_cidr(_list, _cidr_str, _ok) ks_network_list_add_cidr_token(_list, _cidr_str, _ok, NULL)

KS_DECLARE(char *) ks_network_ipv4_mapped_ipv6_addr(const char* ip_str);
KS_DECLARE(ks_status_t) ks_network_list_add_host_mask(ks_network_list_t *list, const char *host, const char *mask_str, ks_bool_t ok);
KS_DECLARE(ks_bool_t) ks_network_list_validate_ip_token(ks_network_list_t *list, uint32_t ip, const char **token);
KS_DECLARE(ks_bool_t) ks_network_list_validate_ip6_token(ks_network_list_t *list, ks_ip_t ip, const char **token);

#define ks_network_list_validate_ip(_list, _ip) ks_network_list_validate_ip_token(_list, _ip, NULL);

#define ks_test_subnet(_ip, _net, _mask) (_mask ? ((_net & _mask) == (_ip & _mask)) : _net ? _net == _ip : 1)

KS_DECLARE(ks_bool_t) ks_check_network_list_ip_cidr(const char *ip_str, const char *cidr_str);
KS_DECLARE(ks_bool_t) ks_check_network_list_ip_token(const char *ip_str, ks_network_list_t *list, const char **token);
#define ks_check_network_list_ip(_i, _l) ks_check_network_list_ip_token(_i, _l, NULL)

KS_END_EXTERN_C
#endif							/* defined(_KS_ACL_H_) */

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

