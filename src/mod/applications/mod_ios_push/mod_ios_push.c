/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * mod_ios_push.c -- ios_push
 *
 */
#include <switch.h>
#include <switch_ssl.h>
#include <sys/select.h> /* select() */
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ios_push_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_ios_push_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_ios_push_load);

#define ios_PUSH_SYNTAX "<token[|token]> [msg]"
static const char *global_cf = "ios_push.conf";
static struct {
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	int running;
	char *host;
	char *cert;
	const char *clientcert;
	int expire;
	int port;
} globals;


struct ios_param{
	switch_core_session_t *session;
	char *data;
};

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_ios_push, mod_ios_push_load, mod_ios_push_shutdown, NULL);


//+++++start++++++ added by yy for ios_push
#define MAX_PAYLOAD_SIZE 256
#define TOKEN_SIZE 32

void DeviceToken2Binary(const char* sz, const int len, unsigned char* const binary, const int size)
{
	int i;
	unsigned int val;
	const char* 	pin;
	char buf[3] = {0};

	assert(size >= TOKEN_SIZE);

	for (i = 0;i < len;i++)
	{
		pin = sz + i * 2;
		buf[0] = pin[0];
		buf[1] = pin[1];

		val = 0;
		sscanf(buf, "%X", &val);
		binary[i] = val;
	}

	return;
}

void DeviceBinary2Token(const unsigned char* data, const int len, char* const token, const int size)
{
	int i;

	assert(size > TOKEN_SIZE * 2);

	for (i = 0;i < len;i++)
	{
		sprintf(token + i * 2, "%02x", data[i]);
	}

	return;
}

void Closesocket(int socket)
{
#ifdef _WIN32
	closesocket(socket);
#else
	close(socket);
#endif
}
/*
 * This callback hands back the password to be used during decryption.
 *
 * buf      : the function will write the password into this buffer
 * size     : the size of "buf"
 * rwflag   : indicates whether the callback is being used for reading/
 *            decryption (0) or writing/encryption (1)
 * userdata : pointer tls_issues_t where the passphrase is stored
 */
static int passwd_cb(char *buf, int size, int rwflag, void *userdata)
{
	if (rwflag == 0) { // reading/decryption
		char *passphrase = (char *)userdata;

		strncpy(buf, passphrase, size);
		buf[size - 1] = '\0';

		return strlen(passphrase);
	}
	return 0;
}

void init_openssl()
{
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	SSL_library_init();
	ERR_load_BIO_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
}

SSL_CTX* init_ssl_context(
		const char* clientcert,
		const char* clientkey, 
		const char* keypwd, 
		const char* cacert)
{
	// set up the ssl context
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
	if (!ctx) {
		return NULL;
	}

	SSL_CTX_set_timeout(ctx,10);

	/* Set callback if we have a passphrase */
	if (keypwd != NULL) {
		SSL_CTX_set_default_passwd_cb(ctx, passwd_cb);
		SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)keypwd);
	}

	// certificate
	if (SSL_CTX_use_certificate_file(ctx, clientcert, SSL_FILETYPE_PEM) <= 0) {
		return NULL;
	}

	// key
	if (SSL_CTX_use_PrivateKey_file(ctx, clientkey, SSL_FILETYPE_PEM) <= 0) {
		return NULL;
	}

	// make sure the key and certificate file match
	if (SSL_CTX_check_private_key(ctx) == 0) {
		return NULL;
	}

	// load ca if exist
	if (cacert) {
		if (!SSL_CTX_load_verify_locations(ctx, cacert, NULL)) {
			return NULL;
		}
	}

	return ctx;
}

int tcp_connect(const char* host, int port)
{
	struct hostent *hp;
	struct sockaddr_in addr;
	int sock = -1;
	int error=-1;
	
    struct timeval  tv;
	struct timeval tm;  
	fd_set set,rset;
	int flag;
	int ret=-1;
	int len;
	len = sizeof(int);
    tv.tv_sec = 10;
    tv.tv_usec= 0; 
	if (!(hp = gethostbyname(host))) {
		return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr = *(struct in_addr*)hp->h_addr_list[0];
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		return -1;
	}
	flag = fcntl(sock,F_GETFL,0);
    flag |= O_NONBLOCK;
    fcntl(sock,F_SETFL,flag);
	if ((ret=connect(sock, (struct sockaddr*)&addr, sizeof(addr))) != 0) {
		tm.tv_sec = 3;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_ZERO(&rset);
		FD_SET(sock, &set);
		FD_SET(sock, &rset);
		if(select(sock+1,&rset, &set, NULL, &tm) > 0)
		{
			getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
			if(error == 0) 
				ret = 0;
			else 
				ret = -2;
		} else {
			ret = -3;
		}
	}
	if(ret < 0){
		return ret;
	}
	else{
		setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char *)&tv,sizeof(struct timeval));
		if(FD_ISSET(sock,&set))
		{
			flag = fcntl(sock,F_GETFL,0);
			flag &= ~O_NONBLOCK;
			fcntl(sock,F_SETFL,flag);
		}
		return sock;
	}
}

SSL* ssl_connect(SSL_CTX* ctx, int socket)
{
	SSL *ssl = SSL_new(ctx);
	BIO *bio = BIO_new_socket(socket, BIO_NOCLOSE);
	SSL_set_bio(ssl, bio, bio);

	if (SSL_connect(ssl) <= 0) {
		return NULL;
	}

	return ssl;
}

int verify_connection(SSL* ssl, const char* peername)
{
	X509 *peer;
	char peer_CN[256] = {0};
	int result = SSL_get_verify_result(ssl);
	if (result != X509_V_OK) {
		fprintf(stderr, "WARNING! ssl verify failed: %d", result);
		return -1;
	}

	peer = SSL_get_peer_certificate(ssl);
	X509_NAME_get_text_by_NID(X509_get_subject_name(peer), NID_commonName, peer_CN, 255);
	if (strcmp(peer_CN, peername) != 0) {
		fprintf(stderr, "WARNING! Server Name Doesn't match, got: %s, required: %s", peer_CN,
				peername);
	}
	return 0;
}

void json_escape(char*  str)
{
	int 	n;
	char  	buf[1024];
	char *found = NULL;

	n = strlen(str) * sizeof(char) + 100;
	assert(n < sizeof(buf));

	strncpy(buf, str, n);
	buf[n] = '\0';
	found = buf;
	while (*found != '\0')
	{
		if('\\' == *found || '"' == *found || '\n' == *found || '/' == *found)
			*str++ = '\\';

		if('\n' == *found)
			*found = 'n';

		*str++ = *found++;
	}

	*str='\0';

	return;
}

// Payload example

// {"aps":{"alert" : "You got your emails.","badge" : 9,"sound" : "default"}}
int build_payload(char* buffer, int* plen, char* msg, int badage, const char * sound)
{
	int n;
	char buf[2048];
	char str[2048] = "{\"aps\":{\"alert\":\"";

	n = strlen(str);

	if (msg)
	{
		strcpy(buf, msg);
		json_escape(buf);
		n = n + sprintf(str+n, "%s", buf);
	}

	n = n + sprintf(str+n, "%s%d", "\",\"badge\":", badage);

	if (sound)
	{
		n = n + sprintf(str+n, "%s", ",\"sound\":\"");
		strcpy(buf, sound);
		json_escape(buf);
		n = n + sprintf(str+n, "%s%s", buf, "\"");
	}

	strcat(str, "}}");

	n = strlen(str);

	if (n > *plen)
	{
		*plen = n;
		return -1;
	}


	if (n < *plen)
	{
		strcpy(buffer, str);
	} else
	{
		strncpy(buffer, str, *plen);
	}

	*plen = n;

	return *plen;
}


int build_output_packet(char* buf, int buflen, 
		const char* tokenbinary,
		char* msg, 
		int badage, 
		const char * sound)
{

	char * pdata = buf;
	int payloadlen = MAX_PAYLOAD_SIZE;

	assert(buflen >= 1 + 2 + TOKEN_SIZE + 2 + MAX_PAYLOAD_SIZE);

	// command
	*pdata = 0;

	// token length
	pdata++;
	*(uint16_t*)pdata = htons(TOKEN_SIZE);

	// token binary
	pdata += 2;
	memcpy(pdata, tokenbinary, TOKEN_SIZE);

	pdata += TOKEN_SIZE;

	if (build_payload(pdata + 2, &payloadlen, msg, badage, sound) < 0)
	{
		msg[strlen(msg) - (payloadlen - MAX_PAYLOAD_SIZE)] = '\0';
		payloadlen = MAX_PAYLOAD_SIZE;
		if (build_payload(pdata + 2, &payloadlen, msg, badage, sound) <= 0)
		{
			return -1;
		}
	}
	*(uint16_t*)pdata = htons(payloadlen);

	return 1 + 2 + TOKEN_SIZE + 2 + payloadlen;
}

int send_message(SSL *ssl, const char* token, char* msg, int badage, const char* sound)
{
	int 		n;
	char 		buf[1 + 2 + TOKEN_SIZE + 2 + MAX_PAYLOAD_SIZE];
	unsigned char 	binary[TOKEN_SIZE];
	int 		buflen = sizeof(buf);

	n = strlen(token);
	DeviceToken2Binary(token, n, binary, TOKEN_SIZE);

	buflen = build_output_packet(buf, buflen, (const char*)binary, msg, badage, sound);
	if (buflen <= 0) {
		return -1;
	}

	return SSL_write(ssl, buf, buflen);
}

int build_output_packet_2(char* buf, int buflen, 
		uint32_t messageid,
		uint32_t expiry, 
		const char* tokenbinary, 
		char* msg, /* message */
		int badage, /* badage */
		const char * sound) /* sound */
{
	char * pdata = buf;
	int payloadlen = MAX_PAYLOAD_SIZE;

	assert(buflen >= 1 + 4 + 4 + 2 + TOKEN_SIZE + 2 + MAX_PAYLOAD_SIZE);

	// command
	*pdata = 1;

	// messageid
	pdata++;
	*(uint32_t*)pdata = messageid;

	// expiry time
	pdata += 4;
	*(uint32_t*)pdata = htonl(expiry);

	// token length
	pdata += 4;
	*(uint16_t*)pdata = htons(TOKEN_SIZE);

	// token binary
	pdata += 2;
	memcpy(pdata, tokenbinary, TOKEN_SIZE);

	pdata += TOKEN_SIZE;

	if (build_payload(pdata + 2, &payloadlen, msg, badage, sound) < 0)
	{
		msg[strlen(msg) - (payloadlen - MAX_PAYLOAD_SIZE)] = '\0';
		payloadlen = MAX_PAYLOAD_SIZE;
		if (build_payload(pdata + 2, &payloadlen, msg, badage, sound) <= 0)
		{
			return -1;
		}
	}

	*(uint16_t*)pdata = htons(payloadlen);

	return 1 + 4 + 4 + 2 + TOKEN_SIZE + 2 + payloadlen;
}

int send_message_2(switch_core_session_t *session, SSL *ssl, const char* token, uint32_t id, uint32_t expire, char* msg, int badage, const char* sound)
{
	int 		i, n;
	char buf[1 + 4 + 4 + 2 + TOKEN_SIZE + 2 + MAX_PAYLOAD_SIZE];
	unsigned char 	binary[TOKEN_SIZE];
	int buflen = sizeof(buf);

	n = strlen(token);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "token length : %d, TOKEN_SIZE = %d\n token = %s\n", n, TOKEN_SIZE, token);
	DeviceToken2Binary(token, n, binary, TOKEN_SIZE);

	for (i = 0; i < TOKEN_SIZE; i++)
		printf("%d ", binary[i]);
	printf("\n");


	buflen = build_output_packet_2(buf, buflen, id, expire,(const char*)binary, msg, badage, sound);

	if (buflen <= 0) {
		return -1;
	}

	n = SSL_write(ssl, buf, buflen);

	return  n;
}

void *SWITCH_THREAD_FUNC ios_wakeup_thread(switch_thread_t *thread, void *obj){
	struct ios_param *ios_att = (struct ios_param *) obj;
	switch_core_session_t *session = ios_att->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *caller_id_number;
	int len;
	char pt_token[128] = {0};

	char * ptr_cur = NULL;
	char * ptr_next = NULL;
	SSL_CTX *ctx = NULL;

	int socket = -1;
	uint32_t msgid = 1;
	uint32_t expire = time(NULL) + global.expire; // expire 1 sec

	SSL *ssl = NULL;
	const char* clientcert = NULL;
	int argc = 0;
	char *argv[3] = { 0 };
	char * msg = NULL;
	char * token = NULL;
	/* Parse application data  */
	if (!zstr(ios_att->data)) {
		argc = switch_separate_string(ios_att->data, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	if (argc > 0) token = argv[0];
	if (argc > 1) msg = argv[1];

	/* backwards compat version, if we have 5, just prepend with db and reparse */
	if (!token) {
		//token = "3cb201c2b937201ce1e0f512c1a8e2512a8f1fadc128b37e78b833174dd6fd94";
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE: ios_push %s\n", ios_PUSH_SYNTAX);
		return NULL;
	}
	/* Parse application data  */
	init_openssl();
	clientcert = switch_core_session_sprintf(session, "%s/%s", SWITCH_GLOBAL_dirs.certs_dir,global.cert);
	
	ctx = init_ssl_context(clientcert, clientcert, "synway", NULL);
	if (!ctx) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "init ssl context failed: %s\n",ERR_reason_error_string(ERR_get_error()));
		return NULL;
	}

	socket = tcp_connect(global.host, global.port);
	if (socket < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "failed to connect to host %s\n",strerror(errno));
		return NULL;
	}

	ssl = ssl_connect(ctx, socket);
	if (!ssl) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "ssl connect failed: %s\n",ERR_reason_error_string(ERR_get_error()));
		Closesocket(socket);
		return NULL;
	}
	/*
	if (verify_connection(ssl, host) != 0) {
		fprintf(stderr, "verify failed\n");
		Closesocket(socket);
		return 1;
	}
	*/
 	
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "main, expire = %d\n", expire);
 	caller_id_number = switch_channel_get_variable(channel, "caller_id_number");
	if (!msg) {
		msg = "hello\nThis is a synway message";
		if(caller_id_number){
			msg = switch_core_session_sprintf(session, "%s(%s)", msg,caller_id_number);
		}
	}

	//const char* token = "458bd85aae1f8132383cfb455597758c228684bcbb14c3b83af61cc9d694f71f";
	//const char* token = "3cb201c2b937201ce1e0f512c1a8e2512a8f1fadc128b37e78b833174dd6fd94";
	ptr_cur = token;
	while (ptr_cur) {
		if ((ptr_next = strchr(ptr_cur, '|'))) {
			len = (int)(ptr_next++ - ptr_cur);
		} else {
			len = (int)strlen(ptr_cur);
		}
		strncpy(pt_token,ptr_cur,len);

		len = send_message_2(session, ssl, pt_token, msgid++, expire, msg, 1, "default");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "after send_message_2, len = %d\n", len);
		if (len <= 0)
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "send failed: %s\n", ERR_reason_error_string(ERR_get_error()));
		}else
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "send sucessfully.");
		}

		ptr_cur = ptr_next;
	}

	//n = recv(socket, buf, sizeof(buf), MSG_DONTWAIT);
	/*
	n = read(socket, buf, sizeof(buf)); 
	printf("from APNS, n = %d\n", n);
	for(i=0; i<n; i++)
		printf("%d ", buf[i]);
	printf("\n");
	*/
/*
	len = SSL_read(ssl, buf, sizeof(buf));

	if(len > 0){
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "from APNS, len = %d\n", len);
	}
	else{
		err = SSL_get_error(ssl, len);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "error from APNS, err = %d\n", err);
	}
 */
	SSL_shutdown(ssl);
	Closesocket(socket);
	return NULL;
}

SWITCH_STANDARD_APP(ios_push_function)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	struct ios_param *iosattr;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_detach_set(thd_attr, 1);
	iosattr = switch_core_session_alloc(session, sizeof(*iosattr));
	iosattr->session = session;
	iosattr->data = switch_core_session_strdup(session, data);
	switch_thread_create(&thread, thd_attr, ios_wakeup_thread, iosattr, pool);
}

static switch_status_t load_config(switch_memory_pool_t *pool) {
	
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		status = SWITCH_STATUS_TERM;
		goto end;
	}

	if ((settings = switch_xml_child(*cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char*)switch_xml_attr_soft(param, "name");
			char *val = (char*)switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "host") && !zstr(val)) {
				globals.host = switch_core_strdup(globals.pool, val);
			}  else if (!strcasecmp(var, "port") && !zstr(val)) {
				globals.port = atoi(val);
			} else if (!strcasecmp(var, "expire") && !zstr(val)) {
				globals.expire = atoi(val);
			} else if (!strcasecmp(var, "cert") && !zstr(val)) {
				globals.cert = switch_core_strdup(globals.pool, val);
			}
		}
	}
	
end:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}


/* Macro expands to: switch_status_t mod_ios_push_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_ios_push_load)
{
	switch_status_t status = SWITCH_STATUS_TERM;
	switch_api_interface_t *api_interface;
	switch_xml_t cfg, xml;
	int count = 0;
	
	globals.pool = pool;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if ((status = load_config(globals.pool)) != SWITCH_STATUS_SUCCESS) return status;

	SWITCH_ADD_APP(app_interface, "ios_push", "ios_push", "ios_push", ios_push_function, ios_PUSH_SYNTAX, SAF_NONE);

end:
	if (xml)
		switch_xml_free(xml);

	return status;
}

/*
   Called when the system shuts down
   Macro expands to: switch_status_t mod_ios_push_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ios_push_shutdown)
{
	/* Cleanup dynamically allocated config settings */

	memset(&globals, 0, sizeof(globals));
	return SWITCH_STATUS_UNLOAD;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
