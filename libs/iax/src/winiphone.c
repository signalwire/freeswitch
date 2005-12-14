/*
 * Miniphone: A simple, command line telephone
 *
 * IAX Support for talking to Asterisk and other Gnophone clients
 *
 * Copyright (C) 1999, Linux Support Services, Inc.
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 */

/* #define	PRINTCHUCK /* enable this to indicate chucked incomming packets */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <process.h>
#include <windows.h>
#include <winsock.h>
#include <mmsystem.h>
#include <malloc.h>
#include "gsm.h"
#include "iax-client.h"
#include "frame.h"
#include "miniphone.h"


struct peer {
	int time;
	gsm gsmin;
	gsm gsmout;

	struct iax_session *session;
	struct peer *next;
};

static struct peer *peers;
static int answered_call = 0;

/* stuff for wave audio device */
HWAVEOUT wout;
HWAVEIN win;

typedef struct whout {
	WAVEHDR w;
	short	data[160];
	struct whout *next;
} WHOUT;

WHOUT *outqueue = NULL;

/* parameters for audio in */
#define	NWHIN 8				/* number of input buffer entries */
/* NOTE the OUT_INTERVAL parameter *SHOULD* be more around 18 to 20 or so, since the packets should
be spaced by 20 milliseconds. However, in practice, especially in Windoze-95, setting it that high
caused underruns. 10 is just ever so slightly agressive, and the receiver has to chuck a packet
every now and then. Thats about the way it should be to be happy. */
#define	OUT_INTERVAL 10		/* number of ms to wait before sending more data to peer */
/* parameters for audio out */
#define	OUT_DEPTH 12		/* number of outbut buffer entries */
#define	OUT_PAUSE_THRESHOLD 2 /* number of active entries needed to start output (for smoothing) */

/* audio input buffer headers */
WAVEHDR whin[NWHIN];
/* audio input buffers */
char bufin[NWHIN][320];

/* initialize the sequence variables for the audio in stuff */
unsigned int whinserial = 1,nextwhin = 1;

static struct peer *find_peer(struct iax_session *);
static void parse_args(FILE *, unsigned char *);
void do_iax_event(FILE *);
void call(FILE *, char *);
void answer_call(void);
void reject_call(void);
static void handle_event(FILE *, struct iax_event *e, struct peer *p);
void parse_cmd(FILE *, int, char **);
void issue_prompt(FILE *);
void dump_array(FILE *, char **);

static char *help[] = {
"Welcome to the miniphone telephony client, the commands are as follows:\n",
"Help\t\t-\tDisplays this screen.",
"Call <Number>\t-\tDials the number supplied.",
"Answer\t\t-\tAnswers an Inbound call.",
"Reject\t\t-\tRejects an Inbound call.",
"Dump\t\t-\tDumps (disconnects) the current call.",
"Dtmf <Digit>\t-\tSends specified DTMF digit.",
"Status\t\t-\tLists the current sessions and their current status.",
"Quit\t\t-\tShuts down the client.",
"",
0
};

static struct peer *most_recent_answer;
static struct iax_session *newcall = 0;

/* holder of the time, relative to startup in system ticks. See our
gettimeofday() implementation */
time_t	startuptime;

/* routine called at exit to shutdown audio I/O and close nicely.
NOTE: If all this isnt done, the system doesnt not handle this
cleanly and has to be rebooted. What a pile of doo doo!! */
void killem(void)
{
	waveInStop(win);
	waveInReset(win);
	waveInClose(win); 
	waveOutReset(wout);
	waveOutClose(wout);
	WSACleanup(); /* dont forget socket stuff too */
	return;
}

/* Win-doze doenst have gettimeofday(). This sux. So, what we did is
provide some gettimeofday-like functionality that works for our purposes.
In the main(), we take a sample of the system tick counter (into startuptime).
This function returns the relative time since program startup, more or less,
which is certainly good enough for our purposes. */
void gettimeofday(struct timeval *tv, struct timezone *tz)
{
	long l = startuptime + GetTickCount();

	tv->tv_sec = l / 1000;
	tv->tv_usec = (l % 1000) * 1000;
	return;
}


static struct peer *find_peer(struct iax_session *session)
{
	struct peer *cur = peers;
	while(cur) {
		if (cur->session == session)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

void
parse_args(FILE *f, unsigned char *cmd)
{
	static char *argv[MAXARGS];
	unsigned char *parse = cmd;
	int argc = 0, t = 0;

	// Don't mess with anything that doesn't exist...
	if(!*parse)
		return;

	memset(argv, 0, sizeof(argv));
	while(*parse) {
		if(*parse < 33 || *parse > 128) {
			*parse = 0, t++;
			if(t > MAXARG) {
				fprintf(f, "Warning: Argument exceeds maximum argument size, command ignored!\n");
				return;
			}
		} else if(t || !argc) {
			if(argc == MAXARGS) {
				fprintf(f, "Warning: Command ignored, too many arguments\n");
				return;
			}
			argv[argc++] = parse;
			t = 0;
		}

		parse++;
	}

	if(argc)
		parse_cmd(f, argc, argv);
}

/* handle all network requests, and a pending scheduled event, if any */
void service_network(int netfd, FILE *f)
{
	fd_set readfd;
	struct timeval dumbtimer;

	/* set up a timer that falls-through */
	dumbtimer.tv_sec = 0;
	dumbtimer.tv_usec = 0;


		for(;;) /* suck everything outa network stuff */
		{
			FD_ZERO(&readfd);
			FD_SET(netfd, &readfd);
			if (select(netfd + 1, &readfd, 0, 0, &dumbtimer) > 0)
			{
				if (FD_ISSET(netfd,&readfd))
				{
					do_iax_event(f);
					(void) iax_time_to_next_event();
				} else break;
			} else break;
		}
		do_iax_event(f); /* do pending event if any */
}


int
main(int argc, char *argv[])
{
	int port;
	int netfd;
 	int c, i;
	FILE *f;
	char rcmd[RBUFSIZE];
	gsm_frame fo;
	WSADATA foop;
	time_t	t;
	WAVEFORMATEX wf;
	WHOUT *wh,*wh1,*wh2;
	unsigned long lastouttick = 0;



	/* get time of day in milliseconds, offset by tick count (see our
	   gettimeofday() implementation) */
	time(&t);
	startuptime = ((t % 86400) * 1000) - GetTickCount();

	f = stdout;
	_dup2(fileno(stdout),fileno(stderr));

	/* start up the windoze-socket layer stuff */
	if (WSAStartup(0x0101,&foop)) {
		fprintf(stderr,"Fatal error: Falied to startup windows sockets\n");
		return -1;
	}


	/* setup the format for opening audio channels */
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 1;
	wf.nSamplesPerSec = 8000;
	wf.nAvgBytesPerSec = 16000;
	wf.nBlockAlign = 2;
	wf.wBitsPerSample = 16;
	wf.cbSize = 0;
	/* open the audio out channel */
	if (waveOutOpen(&wout,0,&wf,0,0,CALLBACK_NULL) != MMSYSERR_NOERROR)
		{
			fprintf(stderr,"Fatal Error: Failed to open wave output device\n");
			return -1;
		}
	/* open the audio in channel */
	if (waveInOpen(&win,0,&wf,0,0,CALLBACK_NULL) != MMSYSERR_NOERROR)
		{
			fprintf(stderr,"Fatal Error: Failed to open wave input device\n");
			waveOutReset(wout);
			waveOutClose(wout);
			return -1;
		}
	/* activate the exit handler */
	atexit(killem);
	/* initialize the audio in buffer structures */
	memset(&whin,0,sizeof(whin));

	if ( (port = iax_init(0) < 0)) {
		fprintf(stderr, "Fatal error: failed to initialize iax with port %d\n", port);
		return -1;
	}


	iax_set_formats(AST_FORMAT_GSM);
	netfd = iax_get_fd();

	fprintf(f, "Text Based Telephony Client.\n\n");
	issue_prompt(f);

	/* main tight loop */
	while(1) {
		/* service the network stuff */
		service_network(netfd,f);
		if (outqueue) /* if stuff in audio output queue, free it up if its available */
		{
			/* go through audio output queue */
			for(wh = outqueue,wh1 = wh2 = NULL,i = 0; wh != NULL; wh = wh->next)
			{
				service_network(netfd,f); /* service network here for better performance */
				/* if last one was removed from queue, zot it here */
				if (i && wh1)
				{ 
					free(wh1);
					wh1 = wh2;
				}
				i = 0; /* reset "last one removed" flag */
				if (wh->w.dwFlags & WHDR_DONE) /* if this one is done */
				{
					/* prepare audio header */
					if ((c = waveOutUnprepareHeader(wout,&wh->w,sizeof(WAVEHDR))) != MMSYSERR_NOERROR)
					{ 
						fprintf(stderr,"Cannot unprepare audio out header, error %d\n",c);
						exit(255);
					}
					if (wh1 != NULL) /* if there was a last one */
					{
						wh1->next = wh->next;
					} 
					if (outqueue == wh) /* is first one, so set outqueue to next one */
					{
						outqueue = wh->next;
					}
					i = 1; /* set 'to free' flag */
				}
				wh2 = wh1;	/* save old,old wh pointer */
				wh1 = wh; /* save the old wh pointer */
			}
		}
		/* go through all audio in buffers, and prepare and queue ones that are currently idle */
		for(i = 0; i < NWHIN; i++)
		{
			service_network(netfd,f); /* service network stuff here for better performance */
			if (!(whin[i].dwFlags & WHDR_PREPARED)) /* if not prepared, do so */
			{
				/* setup this input buffer header */
				memset(&whin[i],0,sizeof(WAVEHDR));
				whin[i].lpData = bufin[i];
				whin[i].dwBufferLength = 320;
				whin[i].dwUser = whinserial++; /* set 'user data' to current serial number */
				/* prepare the buffer */
				if (waveInPrepareHeader(win,&whin[i],sizeof(WAVEHDR)))
				{
					fprintf(stderr,"Unable to prepare header for input\n");
					return -1;
				}
				/* add it to device (queue) */
				if (waveInAddBuffer(win,&whin[i],sizeof(WAVEHDR)))
				{
					fprintf(stderr,"Unable to prepare header for input\n");
					return -1;
				}
			}
			waveInStart(win); /* start it (if not already started) */
		}
		
		/* if key pressed, do command stuff */
		if(_kbhit())
		{
				if ( ( fgets(&*rcmd, 256, stdin))) {
					rcmd[strlen(rcmd)-1] = 0;
					parse_args(f, &*rcmd);
				} else fprintf(f, "Fatal error: failed to read data!\n");

				issue_prompt(f);
		}
		/* do audio input stuff for buffers that have received data from audio in device already. Must
			do them in serial number order (the order in which they were originally queued). */
		if(answered_call) /* send audio only if call answered */
		{
			for(;;) /* loop until all are found */
			{
				for(i = 0; i < NWHIN; i++) /* find an available one that's the one we are looking for */
				{
					service_network(netfd,f); /* service network here for better performance */
					/* if not time to send any more, dont */
					if (GetTickCount() < (lastouttick + OUT_INTERVAL))
					{
						i = NWHIN; /* set to value that WILL exit loop */
						break;
					}
					if ((whin[i].dwUser == nextwhin) && (whin[i].dwFlags & WHDR_DONE)) { /* if audio is ready */

						/* must have read exactly 320 bytes */
						if (whin[i].dwBytesRecorded != whin[i].dwBufferLength)
						{
							fprintf(stderr,"Short audio read, got %d bytes, expected %d bytes\n", whin[i].dwBytesRecorded,
								whin[i].dwBufferLength);
							return -1;
						}
						if(!most_recent_answer->gsmout)
								most_recent_answer->gsmout = gsm_create();

						service_network(netfd,f); /* service network here for better performance */
						/* encode the audio from the buffer into GSM format */
						gsm_encode(most_recent_answer->gsmout, (short *) ((char *) whin[i].lpData), fo);
						if(iax_send_voice(most_recent_answer->session,
							AST_FORMAT_GSM, (char *)fo, sizeof(gsm_frame)) == -1)
									puts("Failed to send voice!"); 
						lastouttick = GetTickCount(); /* save time of last output */

						/* unprepare (free) the header */
						waveInUnprepareHeader(win,&whin[i],sizeof(WAVEHDR));
						/* initialize the buffer */
						memset(&whin[i],0,sizeof(WAVEHDR));
						/* bump the serial number to look for the next time */
						nextwhin++;
						/* exit the loop so that we can start at lowest buffer again */
						break;
					}
				} 
				if (i >= NWHIN) break; /* if all found, get out of loop */
			}
		}

	}
	return 0;
}

void
do_iax_event(FILE *f) {
	int sessions = 0;
	struct iax_event *e = 0;
	struct peer *peer;

	while ( (e = iax_get_event(0))) {
		peer = find_peer(e->session);
		if(peer) {
			handle_event(f, e, peer);
		} else {
			if(e->etype != IAX_EVENT_CONNECT) {
				fprintf(stderr, "Huh? This is an event for a non-existant session?\n");
			}
			sessions++;

			if(sessions >= MAX_SESSIONS) {
				fprintf(f, "Missed a call... too many sessions open.\n");
			}


			if(e->event.connect.callerid && e->event.connect.dnid)
				fprintf(f, "Call from '%s' for '%s'", e->event.connect.callerid, 
				e->event.connect.dnid);
			else if(e->event.connect.dnid) {
				fprintf(f, "Call from '%s'", e->event.connect.dnid);
			} else if(e->event.connect.callerid) {
				fprintf(f, "Call from '%s'", e->event.connect.callerid);
			} else printf("Call from");
			fprintf(f, " (%s)\n", inet_ntoa(iax_get_peer_addr(e->session).sin_addr));

			if(most_recent_answer) {
				fprintf(f, "Incoming call ignored, there's already a call waiting for answer... \
please accept or reject first\n");
				iax_reject(e->session, "Too many calls, we're busy!");
			} else {
				if ( !(peer = malloc(sizeof(struct peer)))) {
					fprintf(f, "Warning: Unable to allocate memory!\n");
					return;
				}

				peer->time = time(0);
				peer->session = e->session;
				peer->gsmin = 0;
				peer->gsmout = 0;

				peer->next = peers;
				peers = peer;

				iax_accept(peer->session);
				iax_ring_announce(peer->session);
				most_recent_answer = peer;
				fprintf(f, "Incoming call!\n");
			}
			iax_event_free(e);
			issue_prompt(f);
		}
	}
}

void
call(FILE *f, char *num)
{
	struct peer *peer;

	if(!newcall)
		newcall = iax_session_new();
	else {
		fprintf(f, "Already attempting to call somewhere, please cancel first!\n");
		return;
	}

	if ( !(peer = malloc(sizeof(struct peer)))) {
		fprintf(f, "Warning: Unable to allocate memory!\n");
		return;
	}

	peer->time = time(0);
	peer->session = newcall;
	peer->gsmin = 0;
	peer->gsmout = 0;

	peer->next = peers;
	peers = peer;

	most_recent_answer = peer;

	iax_call(peer->session, num, 10);
}

void
answer_call(void)
{
	if(most_recent_answer)
		iax_answer(most_recent_answer->session);
	printf("Answering call!\n");
	answered_call = 1;
}

void
dump_call(void)
{
	if(most_recent_answer)
	{
		iax_hangup(most_recent_answer->session,"");
		free(most_recent_answer);
	}
	printf("Dumping call!\n");
	answered_call = 0;
	most_recent_answer = 0;
	answered_call = 0;
	peers = 0;
	newcall = 0;
}

void
reject_call(void)
{
	iax_reject(most_recent_answer->session, "Call rejected manually.");
	most_recent_answer = 0;
}

void
handle_event(FILE *f, struct iax_event *e, struct peer *p)
{
	int len,n;
	WHOUT *wh,*wh1;
	short fr[160];
	static paused_xmit = 0;


	switch(e->etype) {
		case IAX_EVENT_HANGUP:
			iax_hangup(most_recent_answer->session, "Byeee!");
			fprintf(f, "Call disconnected by peer\n");
			free(most_recent_answer);
			most_recent_answer = 0;
			answered_call = 0;
			peers = 0;
			newcall = 0;
			
			break;

		case IAX_EVENT_REJECT:
			fprintf(f, "Authentication was rejected\n");
			break;
		case IAX_EVENT_ACCEPT:
			fprintf(f, "Waiting for answer... RING RING\n");
			issue_prompt(f);
			break;
		case IAX_EVENT_ANSWER:
			answer_call();
 			break;
		case IAX_EVENT_VOICE:
			switch(e->event.voice.format) {
				case AST_FORMAT_GSM:
					if(e->event.voice.datalen % 33) {
						fprintf(stderr, "Weird gsm frame, not a multiple of 33.\n");
						break;
					}

					if (!p->gsmin)
						p->gsmin = gsm_create();

					len = 0;
					while(len < e->event.voice.datalen) {
						if(gsm_decode(p->gsmin, (char *) e->event.voice.data + len, fr)) {
							fprintf(stderr, "Bad GSM data\n");
							break;
						} else {  /* its an audio packet to be output to user */

							/* get count of pending items in audio output queue */
							n = 0; 
							if (outqueue) 
							{	/* determine number of pending out queue items */
								for(wh = outqueue; wh != NULL; wh = wh->next)
								{
									if (!(wh->w.dwFlags & WHDR_DONE)) n++;
								}
							}
							/* if not too many, send to user, otherwise chuck packet */
							if (n <= OUT_DEPTH) /* if not to chuck packet */
							{
								/* malloc the memory for the queue item */
								wh = (WHOUT *) malloc(sizeof(WHOUT));
								if (wh == (WHOUT *) NULL) /* if error, bail */
								{
									fprintf(stderr,"Outa memory!!!!\n");
									exit(255);
								}
								/* initialize the queue entry */
								memset(wh,0,sizeof(WHOUT));
								/* copy the PCM data from the gsm conversion buffer */
								memcpy((char *)wh->data,(char *)fr,sizeof(fr));
								/* set parameters for data */
								wh->w.lpData = (char *) wh->data;
								wh->w.dwBufferLength = 320;
								
								/* prepare buffer for output */
								if (waveOutPrepareHeader(wout,&wh->w,sizeof(WAVEHDR)))
								{
									fprintf(stderr,"Cannot prepare header for audio out\n");
									exit(255);
								}
								/* if not currently transmitting, hold off a couple of packets for 
									smooth sounding output */
								if ((!n) && (!paused_xmit))
								{
									/* pause output (before starting) */
									waveOutPause(wout);
									/* indicate as such */
									paused_xmit = 1;
								}
								/* queue packet for output on audio device */
								if (waveOutWrite(wout,&wh->w,sizeof(WAVEHDR)))
								{
									fprintf(stderr,"Cannot output to wave output device\n");
									exit(255);
								}
								/* if we are paused, and we have enough packets, start audio */
								if ((n > OUT_PAUSE_THRESHOLD) && paused_xmit)
								{
									/* start the output */
									waveOutRestart(wout);
									/* indicate as such */
									paused_xmit = 0;
								}
								/* insert it onto tail of outqueue */
								if (outqueue == NULL) /* if empty queue */
									outqueue = wh; /* point queue to new entry */
								else /* otherwise is non-empty queue */
								{
									wh1 = outqueue;
									while(wh1->next) wh1 = wh1->next; /* find last entry in queue */
									wh1->next = wh; /* point it to new entry */
								}
							} 
#ifdef	PRINTCHUCK
							else printf("Chucking packet!!\n");
#endif
						}
						len += 33;
					}
					break;
				default :
					fprintf(f, "Don't know how to handle that format %d\n", e->event.voice.format);
			}
			break;
		case IAX_EVENT_RINGA:
			break;
		default:
			fprintf(f, "Unknown event: %d\n", e->etype);
			break;
	}
}

void
parse_cmd(FILE *f, int argc, char **argv)
{
	_strupr(argv[0]);
	if(!strcmp(argv[0], "HELP")) {
		if(argc == 1)
			dump_array(f, help);
		else if(argc == 2) {
			if(!strcmp(argv[1], "HELP"))
				fprintf(f, "Help <Command>\t-\tDisplays general help or specific help on command if supplied an arguement\n");
			else if(!strcmp(argv[1], "QUIT"))
				fprintf(f, "Quit\t\t-\tShuts down the miniphone\n");
			else fprintf(f, "No help available on %s\n", argv[1]);
		} else {
			fprintf(f, "Too many arguements for command help.\n");
		}
	} else if(!strcmp(argv[0], "STATUS")) {
		if(argc == 1) {
			int c = 0;
			struct peer *peerptr = peers;

			if(!peerptr)
				fprintf(f, "No session matches found.\n");
			else while(peerptr) {
	 			fprintf(f, "Listing sessions:\n\n");
				fprintf(f, "Session %d\n", ++c);
				fprintf(f, "Session existed for %d seconds\n", (int)time(0)-peerptr->time);
				if(answered_call)
					fprintf(f, "Call answered.\n");
				else fprintf(f, "Call ringing.\n");

				peerptr = peerptr->next;
			}
		} else fprintf(f, "Too many arguments for command status.\n");
	} else if(!strcmp(argv[0], "ANSWER")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command answer\n");
		else answer_call();
	} else if(!strcmp(argv[0], "REJECT")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command reject\n");
		else {
			fprintf(f, "Rejecting current phone call.\n");
			reject_call();
		}
	} else if(!strcmp(argv[0], "CALL")) {
		if(argc > 2)
			fprintf(f, "Too many arguements for command call\n");
		else {
			call(f, argv[1]);
		}
	} else if(!strcmp(argv[0], "DUMP")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command dump\n");
		else {
			dump_call();
		}
	} else if(!strcmp(argv[0], "DTMF")) {
		if(argc > 2)
		{
			fprintf(f, "Too many arguements for command dtmf\n");
			return;
		}
		if (argc < 1)
		{
			fprintf(f, "Too many arguements for command dtmf\n");
			return;
		}
		if(most_recent_answer)
				iax_send_dtmf(most_recent_answer->session,*argv[1]);
	} else if(!strcmp(argv[0], "QUIT")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command quit\n");
		else {
			fprintf(f, "Good bye!\n");
			exit(1);
		}
	} else fprintf(f, "Unknown command of %s\n", argv[0]);
}

void
issue_prompt(FILE *f)
{
	fprintf(f, "TeleClient> ");
	fflush(f);
}

void
dump_array(FILE *f, char **array) {
	while(*array)
		fprintf(f, "%s\n", *array++);
}
