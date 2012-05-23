#include <scgi.h>
#include <scgi_oop.h>

#define connection_construct_common() memset(&handle, 0, sizeof(handle))



SCGIhandle::SCGIhandle(void)
{
	connection_construct_common();
}

SCGIhandle::~SCGIhandle()
{
	if (handle.connected) {
		scgi_disconnect(&handle);
	}
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
	if (handle.connected) {
        return scgi_disconnect(&handle);
    }

	return 0;
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


int SCGIhandle::sendRequest(const char *host, int port, int timeout)
{
	if (!host) {
		return -2;
	}

	if (timeout < 1000) {
		timeout = 1000;
	}
	
	if (scgi_connect(&handle, host, port, timeout) == SCGI_SUCCESS) {
		return (int) scgi_send_request(&handle);
	}

	return -2;
}

char *SCGIhandle::recv(void)
{
	ssize_t len = scgi_recv(&handle, buf, sizeof(buf));
	
	if (len > 0) {
		return (char *)buf;
	}

	return NULL;
}


int SCGIhandle::bind(const char *host, int port)
{
	return (int) scgi_bind(host, port, &handle.sock);
}


int SCGIhandle::accept(void)
{
	scgi_socket_t client_sock;

	if (scgi_accept(handle.sock, &client_sock, NULL) == SCGI_SUCCESS) {
		return (int) client_sock;
	}

	return -1;
}


