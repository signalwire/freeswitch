/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_spidermonkey_etpan.c -- etpan Javascript Module
 *
 */
#include "mod_spidermonkey.h"
#ifdef _MSC_VER
#pragma warning(disable:4142)
#endif
#include <libetpan/libetpan.h>

static const char modname[] = "etpan";

#define B64BUFFLEN 1024
static const char c64[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int write_buf(int fd, char *buf)
{

	int len = (int) strlen(buf);
	if (fd && write(fd, buf, len) != len) {
		close(fd);
		return 0;
	}

	return 1;
}

/* etpan Object */
/*********************************************************************************/
static JSBool etpan_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	return JS_TRUE;
}

static void etpan_destroy(JSContext * cx, JSObject * obj)
{
}

#if 0
static JSBool etpan_my_method(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	return JS_FALSE;
}
#endif

enum etpan_tinyid {
	etpan_NAME
};

static JSBool js_email(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *to = NULL, *from = NULL, *headers, *body = NULL, *file = NULL;
	char *bound = "XXXX_boundary_XXXX";
	char filename[80], buf[B64BUFFLEN];
	int fd = 0, ifd = 0;
	int x = 0, y = 0, bytes = 0, ilen = 0;
	unsigned int b = 0, l = 0;
	unsigned char in[B64BUFFLEN];
	unsigned char out[B64BUFFLEN + 512];
	char *path = NULL;


	if (argc > 3 &&
		(from = JS_GetStringBytes(JS_ValueToString(cx, argv[0]))) &&
		(to = JS_GetStringBytes(JS_ValueToString(cx, argv[1]))) &&
		(headers = JS_GetStringBytes(JS_ValueToString(cx, argv[2]))) &&
		(body = JS_GetStringBytes(JS_ValueToString(cx, argv[3])))
		) {
		if (argc > 4) {
			file = JS_GetStringBytes(JS_ValueToString(cx, argv[4]));
		}
		snprintf(filename, 80, "%smail.%ld%04x", SWITCH_GLOBAL_dirs.temp_dir, time(NULL), rand() & 0xffff);

		if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644))) {
			if (file) {
				path = file;
				if ((ifd = open(path, O_RDONLY)) < 1) {
					return JS_FALSE;
				}

				snprintf(buf, B64BUFFLEN, "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=\"%s\"\n", bound);
				if (!write_buf(fd, buf)) {
					return JS_FALSE;
				}
			}

			if (!write_buf(fd, headers))
				return JS_FALSE;

			if (!write_buf(fd, "\n\n"))
				return JS_FALSE;

			if (file) {
				snprintf(buf, B64BUFFLEN, "--%s\nContent-Type: text/plain\n\n", bound);
				if (!write_buf(fd, buf))
					return JS_FALSE;
			}

			if (!write_buf(fd, body))
				return JS_FALSE;

			if (file) {
				snprintf(buf, B64BUFFLEN, "\n\n--%s\nContent-Type: application/octet-stream\n"
						 "Content-Transfer-Encoding: base64\n"
						 "Content-Description: Sound attachment.\n"
						 "Content-Disposition: attachment; filename=\"%s\"\n\n", bound, file);
				if (!write_buf(fd, buf))
					return JS_FALSE;

				while ((ilen = read(ifd, in, B64BUFFLEN))) {
					for (x = 0; x < ilen; x++) {
						b = (b << 8) + in[x];
						l += 8;
						while (l >= 6) {
							out[bytes++] = c64[(b >> (l -= 6)) % 64];
							if (++y != 72)
								continue;
							out[bytes++] = '\n';
							y = 0;
						}
					}
					if (write(fd, &out, bytes) != bytes) {
						return -1;
					} else
						bytes = 0;

				}

				if (l > 0) {
					out[bytes++] = c64[((b % 16) << (6 - l)) % 64];
				}
				if (l != 0)
					while (l < 6) {
						out[bytes++] = '=', l += 2;
					}
				if (write(fd, &out, bytes) != bytes) {
					return -1;
				}

			}



			if (file) {
				snprintf(buf, B64BUFFLEN, "\n\n--%s--\n.\n", bound);
				if (!write_buf(fd, buf))
					return JS_FALSE;
			}
		}

		if (fd) {
			close(fd);
		}
		if (ifd) {
			close(ifd);
		}
		snprintf(buf, B64BUFFLEN, "/bin/cat %s | /usr/sbin/sendmail -tf \"%s\" %s", filename, from, to);
		system(buf);
		unlink(filename);


		if (file) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Emailed file [%s] to [%s]\n", filename, to);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Emailed data to [%s]\n", to);
		}
		return JS_TRUE;
	}


	return JS_FALSE;
}

static JSFunctionSpec etpan_methods[] = {
//  {"myMethod", odbc_my_method, 1},
	{0}
};

static JSPropertySpec etpan_props[] = {
//  {"name", etpan_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{0}
};


static JSBool etpan_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;

	return res;
}

JSClass etpan_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, etpan_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, etpan_destroy, NULL, NULL, NULL,
	etpan_construct
};


static JSFunctionSpec etpan_functions[] = {
	{"email", js_email, 2},
	{0}
};

switch_status_t etpan_load(JSContext * cx, JSObject * obj)
{
	JS_DefineFunctions(cx, obj, etpan_functions);

	JS_InitClass(cx,
				 obj, NULL, &etpan_class, etpan_construct, 3, etpan_props, etpan_methods, etpan_props, etpan_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t etpan_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ etpan_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	*module_interface = &etpan_module_interface;
	return SWITCH_STATUS_SUCCESS;
}

/* sample code from smtp send example*/
#if 0
/* globals */
char *smtp_server;
uint16_t smtp_port = 25;
char *smtp_user;
char *smtp_password;
char *smtp_from;
int smtp_tls = 0;
int smtp_esmtp = 1;

struct mem_message {
	char *data;
	size_t len;
	MMAPString *mstring;
};

#define BLOCKSIZE 4096

int collect(struct mem_message *message)
{
	struct stat sb;
	int len;

	memset(message, 0, sizeof(struct mem_message));

#ifndef MMAP_UNAVAILABLE
	/* if stdin is a file whose size is known, try to mmap it */
	if (!fstat(0, &sb) && S_ISREG(sb.st_mode) && sb.st_size >= 0) {
		message->len = sb.st_size;
		if ((message->data = mmap(NULL, message->len, PROT_READ, MAP_SHARED, STDIN_FILENO, 0)) != MAP_FAILED)
			return 0;
	}
#endif

	/* read the buffer from stdin by blocks, until EOF or error.
	   save the message in a mmap_string */
	if ((message->mstring = mmap_string_sized_new(BLOCKSIZE)) == NULL) {
		perror("mmap_string_new");
		goto error;
	}
	message->len = 0;

	while ((len = read(STDIN_FILENO, message->mstring->str + message->len, BLOCKSIZE)) > 0) {
		message->len += len;
		/* reserve room for next block */
		if ((mmap_string_set_size(message->mstring, message->len + BLOCKSIZE)) == NULL) {
			perror("mmap_string_set_size");
			goto error;
		}
	}

	if (len == 0) {
		message->data = message->mstring->str;
		return 0;				/* OK */
	}

	perror("read");

  error:
	if (message->mstring != NULL)
		mmap_string_free(message->mstring);
	return -1;
}

char *guessfrom()
{
#ifndef _MSC_VER
	uid_t uid;
	struct passwd *pw;
	char hostname[256];
	int len;
	char *gfrom;

	if (gethostname(hostname, sizeof(hostname))) {
		perror("gethostname");
		return NULL;
	}
	hostname[sizeof(hostname) - 1] = '\0';

	uid = getuid();
	pw = getpwuid(uid);

	len = ((pw != NULL) ? strlen(pw->pw_name) : 12)
		+ strlen(hostname) + 2;

	if ((gfrom = malloc(len)) == NULL) {
		perror("malloc");
		return NULL;
	}
	if (pw != NULL && pw->pw_name != NULL)
		snprintf(gfrom, len, "%s@%s", pw->pw_name, hostname);
	else
		snprintf(gfrom, len, "#%u@%s", uid, hostname);
	return gfrom;
#else
	return NULL;
#endif
}

void release(struct mem_message *message)
{
	if (message->mstring != NULL)
		mmap_string_free(message->mstring);
#ifndef MMAP_UNAVAILABLE
	else if (message->data != NULL)
		munmap(message->data, message->len);
#endif
}

int send_message(char *data, size_t len, char **rcpts)
{
	int s = -1;
	int ret;
	char **r;
	int esmtp = 0;
	mailsmtp *smtp = NULL;

	if ((smtp = mailsmtp_new(0, NULL)) == NULL) {
		perror("mailsmtp_new");
		goto error;
	}

	/* first open the stream */
	if ((ret = mailsmtp_socket_connect(smtp,
									   (smtp_server != NULL ? smtp_server : "localhost"),
									   smtp_port)) != MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_socket_connect: %s\n", mailsmtp_strerror(ret));
		goto error;
	}

	/* then introduce ourselves */
	if (smtp_esmtp && (ret = mailesmtp_ehlo(smtp)) == MAILSMTP_NO_ERROR)
		esmtp = 1;
	else if (!smtp_esmtp || ret == MAILSMTP_ERROR_NOT_IMPLEMENTED)
		ret = mailsmtp_helo(smtp);
	if (ret != MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_helo: %s\n", mailsmtp_strerror(ret));
		goto error;
	}

	if (esmtp && smtp_tls && (ret = mailsmtp_socket_starttls(smtp)) != MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_starttls: %s\n", mailsmtp_strerror(ret));
		goto error;
	}

	if (esmtp && smtp_user != NULL &&
		(ret = mailsmtp_auth(smtp, smtp_user, (smtp_password != NULL) ? smtp_password : ""))
		!= MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_auth: %s: %s\n", smtp_user, mailsmtp_strerror(ret));
		goto error;
	}

	/* source */
	if ((ret = (esmtp ?
				mailesmtp_mail(smtp, smtp_from, 1, "etPanSMTPTest") :
				mailsmtp_mail(smtp, smtp_from))) != MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_mail: %s, %s\n", smtp_from, mailsmtp_strerror(ret));
		goto error;
	}

	/* recipients */
	for (r = rcpts; *r != NULL; r++) {
		if ((ret = (esmtp ?
					mailesmtp_rcpt(smtp, *r,
								   MAILSMTP_DSN_NOTIFY_FAILURE | MAILSMTP_DSN_NOTIFY_DELAY,
								   NULL) : mailsmtp_rcpt(smtp, *r))) != MAILSMTP_NO_ERROR) {
			fprintf(stderr, "mailsmtp_rcpt: %s: %s\n", *r, mailsmtp_strerror(ret));
			goto error;
		}
	}

	/* message */
	if ((ret = mailsmtp_data(smtp)) != MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_data: %s\n", mailsmtp_strerror(ret));
		goto error;
	}
	if ((ret = mailsmtp_data_message(smtp, data, len)) != MAILSMTP_NO_ERROR) {
		fprintf(stderr, "mailsmtp_data_message: %s\n", mailsmtp_strerror(ret));
		goto error;
	}
	mailsmtp_free(smtp);
	return 0;

  error:
	if (smtp != NULL)
		mailsmtp_free(smtp);
	if (s >= 0)
		close(s);
	return -1;
}

int main(int argc, char **argv)
{
	struct mem_message message;
	int index, r;

	static struct option long_options[] = {
		{"server", 1, 0, 's'},
		{"port", 1, 0, 'p'},
		{"user", 1, 0, 'u'},
		{"password", 1, 0, 'v'},
		{"from", 1, 0, 'f'},
		{"tls", 0, 0, 'S'},
		{"no-esmtp", 0, 0, 'E'},
	};

	while (1) {
		if ((r = getopt_long(argc, argv, "s:p:u:v:f:SE", long_options, &index)) < 0)
			break;
		switch (r) {
		case 's':
			if (smtp_server != NULL)
				free(smtp_server);
			smtp_server = strdup(optarg);
			break;
		case 'p':
			smtp_port = (uint16_t) strtoul(optarg, NULL, 10);
			break;
		case 'u':
			if (smtp_user != NULL)
				free(smtp_user);
			smtp_user = strdup(optarg);
			break;
		case 'v':
			if (smtp_password != NULL)
				free(smtp_password);
			smtp_password = strdup(optarg);
			break;
		case 'f':
			if (smtp_from != NULL)
				free(smtp_from);
			smtp_from = strdup(optarg);
			break;
		case 'S':
			smtp_tls = 1;
			break;
		case 'E':
			smtp_esmtp = 0;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		fprintf(stderr, "usage: smtpsend [-f from] [-u user] [-v password] [-s server] [-p port] [-S] <rcpts>...\n");
		return EXIT_FAILURE;
	}

	if (smtp_from == NULL && (smtp_from = guessfrom()) == NULL) {
		fprintf(stderr, "can't guess a valid from, please use -f option.\n");
		return EXIT_FAILURE;
	}

	/* reads message from stdin */
	if (collect(&message))
		return EXIT_FAILURE;

	send_message(message.data, message.len, argv);

	release(&message);
	return EXIT_SUCCESS;
}

#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
