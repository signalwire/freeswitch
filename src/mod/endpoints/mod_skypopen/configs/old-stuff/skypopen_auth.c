//gcc -Wall -ggdb skypopen_auth.c -o skypopen_auth -lX11
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>

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
	printf("Skype instance found on display '%s', with id #%d\n", dispname, (unsigned int) SkypopenHandles->skype_win);
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

int main(int argc, char *argv[])
{

	struct SkypopenHandles SkypopenHandles;
	char buf[512];
	Display *disp = NULL;
	Window root = -1;
	Window win = -1;

	if (argc == 2)
		dispname = argv[1];
	else
		dispname = ":0.0";

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

		snprintf(buf, 512, "PROTOCOL 6");
		if (!skypopen_send_message(&SkypopenHandles, buf)) {
			printf("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch skypopen_auth again\n");
			return -1;
		}

		/* perform an events loop */
		XEvent an_event;
		char buf[21];			/*  can't be longer */
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

				if (i < 20) {	/* last fragment */
					unsigned int howmany;

					howmany = strlen(b) + 1;

					printf("RECEIVED==> %s\n", b);
					memset(buffer, '\0', 17000);
				}

				break;
			default:
				break;
			}
		}
	} else {
		printf("Skype client not found on display '%s'. Please run/restart Skype manually and launch skypopen_auth again\n\n\n", dispname);
		return -1;
	}
	return 0;

}
