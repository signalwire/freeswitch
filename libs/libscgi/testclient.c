#include <scgi.h>


int main(int argc, char *argv[])
{
	char buf[16336] = "";
	ssize_t len;
	scgi_handle_t handle = { 0 };
	char *ip;
	int port = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: testclient <ip> <port>\n");
		exit(-1);
	}

	ip = argv[1];
	port = atoi(argv[2]);



	scgi_add_param(&handle, "REQUEST_METHOD", "POST");
	scgi_add_param(&handle, "REQUEST_URI", "/deepthought");
	scgi_add_param(&handle, "TESTING", "TRUE");
	scgi_add_param(&handle, "TESTING", "TRUE");
	scgi_add_body(&handle, "What is the answer to life?");


	scgi_connect(&handle, ip, port, 10000);

	scgi_send_request(&handle);

	
	while((len = scgi_recv(&handle, buf, sizeof(buf))) > 0) {
		printf("READ [%s]\n", buf);
	}

	scgi_disconnect(&handle);

}
