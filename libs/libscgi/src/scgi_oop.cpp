#include <scgi.h>
#include <scgi_oop.h>

#define connection_construct_common() memset(&handle, 0, sizeof(handle))



SCGIhandle::SCGIhandle(void)
{
	connection_construct_common();
}

SCGIhandle::~SCGIhandle()
{

	scgi_disconnect(&handle);
	scgi_safe_free(data_buf);
	buflen = 0;
	bufsize = 0;

}

int SCGIhandle::socketDescriptor()
{
	if (handle.connected) {
        return (int) handle.sock;
    }

	return -1;
}


int SCGIhandle::disconnect()
{
	return scgi_disconnect(&handle);
}

int SCGIhandle::connected()
{
	return handle.connected;
}

int SCGIhandle::addParam(const char *name, const char *value)
{
	return (int) scgi_add_param(&handle, name, value);
}

int SCGIhandle::addBody(const char *value)
{
	return (int) scgi_add_body(&handle, value);
}

char *SCGIhandle::getBody()
{
	return handle.body;
}

char *SCGIhandle::getParam(const char *name)
{
	return (char *) scgi_get_param(&handle, name);
}

char *SCGIhandle::sendRequest(const char *host, int port, int timeout)
{
	ssize_t len;

	if (!host) {
		return 0;
	}

	if (timeout < 1000) {
		timeout = 1000;
	}
	
	if (scgi_connect(&handle, host, port, timeout) == SCGI_SUCCESS) {
		if (scgi_send_request(&handle) == SCGI_SUCCESS) {
			while((len = scgi_recv(&handle, buf, sizeof(buf))) > 0) {
				if (buflen + len > bufsize) {
					bufsize = buflen + len + 1024;
					void *tmp = realloc(data_buf, bufsize);
					assert(tmp);
					data_buf = (char *)tmp;

					*(data_buf+buflen) = '\0';
				}
				snprintf(data_buf+buflen, bufsize-buflen, "%s", buf);
				buflen += len;
			}

			return data_buf;
		}
	}

	return (char *) "";
}

int SCGIhandle::bind(const char *host, int port)
{
	return (scgi_bind(host, port, &server_sock) == SCGI_SUCCESS) ? 1 : 0;
}


int SCGIhandle::accept(void)
{
	scgi_socket_t client_sock;

	if (scgi_accept(server_sock, &client_sock, NULL) == SCGI_SUCCESS) {
		if (scgi_parse(client_sock, &handle) == SCGI_SUCCESS) {
			return 1;
		}

		closesocket(client_sock);
	}

	return 0;
}


int SCGIhandle::respond(char *msg)
{
	int b = write(handle.sock, msg, strlen(msg));
	scgi_disconnect(&handle);
	scgi_safe_free(data_buf);
	buflen = 0;
	bufsize = 0;
	return b;
}
