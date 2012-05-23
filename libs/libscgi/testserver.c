#include <scgi.h>

static void callback(scgi_socket_t server_sock, scgi_socket_t *client_sock, struct sockaddr_in *addr)
{
	scgi_handle_t handle = { 0 };

	if (scgi_parse(*client_sock, &handle) == SCGI_SUCCESS) {
		scgi_param_t *pp;

		*client_sock = SCGI_SOCK_INVALID;

		for(pp = handle.params; pp; pp = pp->next) { 
			printf("HEADER: [%s] VALUE: [%s]\n", pp->name, pp->value);
		}
	
		if (handle.body) {
			printf("\n\nBODY:\n%s\n\n", handle.body);
		}
	
		scgi_disconnect(&handle);
	}

}

int main(int argc, char *argv[])
{
	char *ip;
	int port = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: testserver <ip> <port>\n");
		exit(-1);
	}

	ip = argv[1];
	port = atoi(argv[2]);


	scgi_listen(ip, port, callback);



}
