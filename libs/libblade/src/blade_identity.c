/*
 * Copyright (c) 2017, Shane Bryldt
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

#include "blade.h"

struct blade_identity_s {
	const char *uri;

	const char *components;

	const char *scheme;
	const char *user;
	const char *host;
	const char *port;
	ks_port_t portnum;
	const char *path;
	ks_hash_t *parameters;
};

// @todo missed a structure to use cleanup callbacks
static void blade_identity_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_identity_t *bi = (blade_identity_t *)ptr;

	ks_assert(bi);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (bi->uri) ks_pool_free(&bi->uri);
		if (bi->components) ks_pool_free(&bi->components);
		if (bi->parameters) ks_hash_destroy(&bi->parameters);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_identity_create(blade_identity_t **biP, ks_pool_t *pool)
{
	blade_identity_t *bi = NULL;

	ks_assert(biP);
	ks_assert(pool);

	bi = ks_pool_alloc(pool, sizeof(blade_identity_t));

	ks_pool_set_cleanup(bi, NULL, blade_identity_cleanup);

	*biP = bi;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_identity_destroy(blade_identity_t **biP)
{
	ks_assert(biP);
	ks_assert(*biP);

	ks_pool_free(biP);

	return KS_STATUS_SUCCESS;
}

void blade_identity_reset(blade_identity_t *bi)
{
	ks_assert(bi);

	bi->scheme = NULL;
	bi->user = NULL;
	bi->host = NULL;
	bi->port = NULL;
	bi->portnum = 0;
	bi->path = NULL;

	if (bi->uri) {
		ks_pool_free(&bi->uri);
		ks_pool_free(&bi->components);
	}
	if (bi->parameters) ks_hash_destroy(&bi->parameters);
}

KS_DECLARE(ks_status_t) blade_identity_parse(blade_identity_t *bi, const char *uri)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	char *tmp = NULL;
	char *tmp2 = NULL;
	char terminator = '\0';
	ks_pool_t *pool = NULL;

	ks_assert(bi);
	ks_assert(uri);

	ks_log(KS_LOG_DEBUG, "Parsing URI: %s\n", uri);

	pool = ks_pool_get(bi);

	blade_identity_reset(bi);

	bi->uri = ks_pstrdup(pool, uri);
	bi->components = tmp = ks_pstrdup(pool, uri);

	// Supported components with pseudo regex
	// <scheme:[//]> [user@] <host> [:port] [/path] [;param1=value1] [;param2=value2]

	// scheme is mandatory to simplify the parser for now, it must start with mandatory: <scheme>:
	bi->scheme = tmp;
	if (!(tmp = strchr(tmp, ':'))) {
		ret = KS_STATUS_FAIL;
		goto done;
	}
	// if found, null terminate scheme portion
	*tmp++ = '\0';
	// may have trailing '/' characters which are optional, this is not perfect it should probably only match a count of 0 or 2 explicitly
	while (*tmp && *tmp == '/') ++tmp;
	// must have more data to define at least a host
	if (!*tmp) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	if (!(*bi->scheme)) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	// next component may be the user or the host, so it may start with optional: <user>@
	// or it may skip to the host, which may be terminated by an optional port, optional path, optional parameters, or the end of the uri
	// which means checking if an '@' appears before the next ':' for port, '/' for path, or ';' for parameters, if @ appears at all then it appears before end of the uri
	// @todo need to account for host being encapsulated by '[' and ']' as in the case of an IPV6 host to distinguish from the port, but for simplicity allow any
	// host to be encapsulated in which case if the string starts with a '[' here, then it MUST be the host and it MUST be terminated with the matching ']' rather than other
	// optional component terminators
	if (!(tmp2 = strpbrk(tmp, "@:/;"))) {
		// none of the terminators are found, treat the remaining string as a host
		bi->host = tmp;
		goto done;
	}

	// grab the terminator and null terminate for the next component
	terminator = *tmp2;
	*tmp2++ = '\0';

	if (terminator == '@') {
		// if the terminator was an '@', then we have a user component before the host
		bi->user = tmp;

		tmp = tmp2;

		if (!(*bi->user)) {
			ret = KS_STATUS_FAIL;
			goto done;
		}

		// repeat the same as above, except without looking for '@', to find only the end of the host, parsing to the same point as above if user was not found
		if (!(tmp2 = strpbrk(tmp, ":/;"))) {
			// none of the terminators are found, treat the remaining string as a host
			bi->host = tmp;
			goto done;
		}

		// grab the terminator and null terminate for the next component
		terminator = *tmp2;
		*tmp2++ = '\0';
	}

	// at this point the user portion has been parsed if it exists, the host portion has been terminated, and there is still data remaining to parse
	// @todo need to account for host being encapsulated by '[' and ']' as in the case of an IPV6 host to distinguish from the port, but for simplicity allow any
	// host to be encapsulated in which case the terminator MUST be the closing ']'
	bi->host = tmp;
	tmp = tmp2;

	if (!(*bi->host)) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	if (terminator == ':') {
		// port terminator
		bi->port = tmp;

		// next component must be the port, which may be terminated by an optional path, optional parameters, or the end of the uri
		// which means checking if a '/' for path, or ';' for parameters
		if (!(tmp2 = strpbrk(tmp, "/;"))) {
			// none of the terminators are found, treat the remaining string as a port
			goto done;
		}

		terminator = *tmp2;
		*tmp2++ = '\0';

		tmp = tmp2;

		if (!(*bi->port)) {
			ret = KS_STATUS_FAIL;
			goto done;
		}

		// @todo sscanf bi->port into bi->portnum and validate that it is a valid port number
	}

	if (terminator == '/') {
		// path terminator
		bi->path = tmp;

		// next component must be the path, which may be terminated by optional parameters, or the end of the uri
		// which means checking ';' for parameters
		if (!(tmp2 = strpbrk(tmp, ";"))) {
			// none of the terminators are found, treat the remaining string as a path
			goto done;
		}

		terminator = *tmp2;
		*tmp2++ = '\0';

		tmp = tmp2;

		if (!(*bi->path)) {
			ret = KS_STATUS_FAIL;
			goto done;
		}
	}

	if (terminator == ';') {
		// parameter terminator
		do {
			char *key = NULL;
			char *value = NULL;

			// next component must be the parameter key, which must be terminated by mandatory '=', end of the uri is an error
			// which means checking '=' for key terminator
			key = tmp;
			if (!(tmp = strpbrk(tmp, "="))) {
				ret = KS_STATUS_FAIL;
				goto done;
			}
			*tmp++ = '\0';

			// next component must be the parameter value, which may be terminated by another parameter terminator ';', or the end of the uri
			// if it is the end of the uri, then the parameter loop will be exited
			value = tmp;
			if ((tmp = strpbrk(tmp, ";"))) {
				*tmp++ = '\0';
			}

			// create th parameters hash if it does not already exist and add the parameter entry to it, note the key and value are both actually part
			// of the duplicated uri for components and will be cleaned up with the single string so the hash must not free the key or value itself
			if (!bi->parameters) ks_hash_create(&bi->parameters, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
			ks_hash_insert(bi->parameters, (void *)key, (void *)value);
		} while (tmp);
	}

done:
	if (ret != KS_STATUS_SUCCESS) blade_identity_reset(bi);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_identity_uri_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->uri;
}

KS_DECLARE(const char *) blade_identity_scheme_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->scheme;
}

KS_DECLARE(const char *) blade_identity_user_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->user;
}

KS_DECLARE(const char *) blade_identity_host_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->host;
}

KS_DECLARE(const char *) blade_identity_port_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->port;
}

KS_DECLARE(ks_port_t) blade_identity_portnum_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->portnum;
}

KS_DECLARE(const char *) blade_identity_path_get(blade_identity_t *bi)
{
	ks_assert(bi);

	return bi->path;
}

KS_DECLARE(const char *) blade_identity_parameter_lookup(blade_identity_t *bi, const char *key)
{
	ks_assert(bi);
	ks_assert(key);

	if (!bi->parameters) return NULL;

	return (const char *)ks_hash_search(bi->parameters, (void *)key, KS_UNLOCKED);
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
