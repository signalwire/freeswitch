/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: dvdongle2.c
  AUTHOR......: David Rowe                                           
  DATE CREATED: 28 Oct 2010
                                                                             
  Program to encode and decode raw speech samples using the AMBE codec
  implemented on a DV Dongle.

  The DV Dongle connects to a USB port and provides encoding and
  decoding of compressed audio using the DVSI AMBE2000 full duplex
  vocoder DSP chip.
                       
  Refs: 

    [1] http://www.dvdongle.com/
    [2] http://www.moetronix.com/files/dvdongletechref100.pdf
    [3] http://www.dvsinc.com/manuals/AMBE-2000_manual.pdf
    [4] http://www.moetronix.com/files/ambetest103.zip

  Serial code based on ser.c sample from http://www.captain.at

  Compile with:

    gcc dvdongle2.c -o dvdongle2 -Wall -g -O2

  Note: This program is not very stable, it sometimes stops part way
  through processing an utterance.  I made it just good enough to work
  most of the time, as my purpose was just to process a few sample
  files.


\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 1990-2010 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>  
#include <stdio.h>  
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <fcntl.h>  
#include <errno.h>  
#include <termios.h>

#define MAX_STR                  1024
#define LEN_TARGET_NAME_RESPONSE 14
#define N                        160

/* message parsing state machine states */

#define MSGSTATE_HDR1 0		
#define MSGSTATE_HDR2 1
#define MSGSTATE_DATA 2	    

#define LENGTH_MASK 0x1FFF    /* mask for message length            */
#define TYPE_MASK   0xE0      /* mask for upper byte of header      */
#define TYPE_C      0x20      /* compressed speech from target      */
#define TYPE_UC     0x40      /* uncompressed speech from target    */

#define MAX_MSG_LEN 8192

/* Control items sent to DV Dongle */

char target_name[]     = {0x04, 0x20, 0x01, 0x00};

/* note [2] appears to be in error, specifies run as 0x02, stop as 0x01 */

char run_state_stop[]  = {0x05, 0x00, 0x18, 0x00, 0x00};
char run_state_run[]   = {0x05, 0x00, 0x18, 0x00, 0x01};

/* Control item codes from DV Dongle */

char data_item_0[]     = {0x42, 0x81};
char data_item_1[]     = {0x32, 0xa0};
char run_state[]       = {0x05, 0x00};
char idle[]            = {0x00, 0x00};

typedef struct {
    short header;
    char  power;
    char  control1;
    short rate[5];
    short unused[3];
    short dtmf;
    short control2;
    short channel_data[12];
} COMPRESSED;

COMPRESSED c_in;
COMPRESSED c_out;
FILE *fin, *fout, *f;
int    fd, c_msg, uc_msg;

int initport(int fd) {
    struct termios options;

    // Set the options for the port...

    cfmakeraw(&options);
    cfsetispeed(&options, B230400);
    cfsetospeed(&options, B230400);
    options.c_cflag |= (CLOCAL | CREAD);
    tcsetattr(fd, TCSANOW, &options);

    return 1;
}

int getbaud(int fd) {
    struct termios termAttr;
    int     inputSpeed = -1;
    speed_t baudRate;

    tcgetattr(fd, &termAttr);

    /* Get the input speed */

    baudRate = cfgetispeed(&termAttr);
    switch (baudRate) {
	case B0:      inputSpeed = 0; break;
	case B50:     inputSpeed = 50; break;
	case B110:    inputSpeed = 110; break;
	case B134:    inputSpeed = 134; break;
	case B150:    inputSpeed = 150; break;
	case B200:    inputSpeed = 200; break;
	case B300:    inputSpeed = 300; break;
	case B600:    inputSpeed = 600; break;
	case B1200:   inputSpeed = 1200; break;
	case B1800:   inputSpeed = 1800; break;
	case B2400:   inputSpeed = 2400; break;
	case B4800:   inputSpeed = 4800; break;
	case B9600:   inputSpeed = 9600; break;
	case B19200:  inputSpeed = 19200; break;
	case B38400:  inputSpeed = 38400; break;
	case B57600:  inputSpeed = 38400; break;
	case B115200:  inputSpeed = 38400; break;
	case B230400:  inputSpeed = 230400; break;
    }

    return inputSpeed;
}

void write_dongle(int fd, char *data, int len) {
    int n;
    //printf("  writing %d bytes\n", len);
    n = write(fd, data, len);
    if (n < 0) {
	perror("write failed");
	exit(1);
    }
}

void read_dongle(int fd, char *data, int len) {
    int n;
    //printf("  reading %d bytes  \n", len);

    n = read(fd, data, len);
    if (n < 0) {
	perror("read failed");
	exit(1);
    }
    //printf("  read %d bytes\n", len);
}

void parse_message(int msg_type, int msg_len, char msg_data[]) {
    short buf[N];
    COMPRESSED *c_out;

    //printf("msg_type: 0x%02x  msg_len: %d\n", msg_type, msg_len); 

    /* echo compressed speech frames back to target */

    if (msg_type == TYPE_C) {
	c_out = (COMPRESSED*)msg_data;
#ifdef TMP
	printf("control1 0x%04x\n", c_out->control1 & 0xff);
	printf("rate[0]  0x%04x\n", c_out->rate[0]);
	printf("rate[1]  0x%04x\n", c_out->rate[1]);
	printf("rate[2]  0x%04x\n", c_out->rate[2]);
	printf("rate[3]  0x%04x\n", c_out->rate[3]);
	printf("rate[4]  0x%04x\n", c_out->rate[4]);
	printf("control2 0x%04x\n", c_out->control2 & 0xffff);
	printf("cd[0]    0x%04x\n", c_out->channel_data[0] & 0xffff);
	printf("cd[1]    0x%04x\n", c_out->channel_data[1] & 0xffff);
	printf("cd[2]    0x%04x\n", c_out->channel_data[2] & 0xffff);
	printf("cd[3]    0x%04x\n", c_out->channel_data[3] & 0xffff);
	printf("cd[4]    0x%04x\n", c_out->channel_data[4] & 0xffff);
	printf("cd[5]    0x%04x\n", c_out->channel_data[5] & 0xffff);
	printf("cd[6]    0x%04x\n", c_out->channel_data[6] & 0xffff);
	printf("uc_msg %d\n", uc_msg);
#endif	
	printf("bit errors %d\n", c_out->unused[2]);
	memcpy(&c_in.channel_data, 
	       &c_out->channel_data, 
	       sizeof(c_in.channel_data));
		
	write_dongle(fd, data_item_1, sizeof(data_item_1));
	write_dongle(fd, (char*)&c_in, sizeof(c_in));
	
	c_msg++;
    }

    /* write speech buffers to disk */

    if (msg_type == TYPE_UC) {
       
	if (fout != NULL) {
	    fwrite(msg_data, sizeof(char), msg_len-2, fout);
	    printf("msg_len %d\n", msg_len);
	}

	if (fin != NULL)
	    fread(buf, sizeof(short), N, fin);
	else
	    memset(buf, 0, sizeof(buf));
	
	write_dongle(fd, data_item_0, sizeof(data_item_0));
	write_dongle(fd, (char*)buf, sizeof(buf));
	
	uc_msg++;
    }
}

int main(int argc, char **argv) {
    char   response[MAX_STR];
    int    i;
    int    state, next_state;
    short  header;
    int    msg_type, msg_length;
    char   msg_data[MAX_MSG_LEN];
    int    n, length;
    int    r;

    char   data;

    f = fopen("/tmp/log.txt", "wt");
    assert(f != NULL);

    /* open and configure serial port */

    fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
	perror("open_port: Unable to open /dev/ttyS0 - ");
	exit(1);
    } else {
	fcntl(fd, F_SETFL, 0);
    }
	
    initport(fd);

    fin = NULL;
    if (argc >= 2) {
	fin = fopen(argv[1],"rb");
	assert(fin != NULL);
    }
    fout = NULL;
    if (argc == 3) {
	fout = fopen(argv[2],"wb");
	assert(fout != NULL);
    }

    /* check DV Dongle is alive */

    write_dongle(fd, target_name, sizeof(target_name));
    read_dongle(fd, response, LEN_TARGET_NAME_RESPONSE);
    if (strcmp(&response[4],"DV Dongle") != 0) {
	printf("DV Dongle not responding\n");
	exit(1);
    }
    printf("Found DV Dongle....\n");

    c_in.header    = 0x13ec;
    c_in.power     = 0x0;
    c_in.control1  = 0x0;

#define RATE2000
#ifdef RATE2000
    c_in.rate[0]   = 0x0028;  /* 2000 bit/s, no FEC */
    c_in.rate[1]   = 0x0000;
    c_in.rate[2]   = 0x0000;
    c_in.rate[3]   = 0x0000;
    c_in.rate[4]   = 0x6248;
#endif

#ifdef RATE3600_1200
    c_in.rate[0]   = 0x5048;  /* 3600 bit/s, 1200 bit/s FEC */
    c_in.rate[1]   = 0x0001;
    c_in.rate[2]   = 0x0000;
    c_in.rate[3]   = 0x2412;
    c_in.rate[4]   = 0x6860;
#endif

    c_in.unused[0] = 0x0; 
    c_in.unused[1] = 0x0;
    c_in.unused[2] = 0x0;
    c_in.dtmf      = 0x00ff;
    c_in.control2  = 0x8000;

    /* put codec in run mode */

    write_dongle(fd, run_state_run, sizeof(run_state_run));
    //write_dongle(fd, data_item_1, sizeof(data_item_1));
    //write_dongle(fd, (char*)&c_in, sizeof(c_in));

    state = MSGSTATE_HDR1;
    header = msg_type = msg_length = n = length = 0;
    c_msg = uc_msg = 0;

    for(i=0; i<100000; i++) {
	/* 
	   We can only reliably read one byte at a time.  Until I
	   realised this there was "much wailing and gnashing of
	   teeth".  Trying to read() n bytes read() returns n but may
	   actually reads some number between 1 and n.  So it may only
	   read 1 byte int data[] but return n.
	*/
	r = read(fd, &data, 1);
	assert(r == 1);

	/* used state machine design from ambetest103.zip, SerialPort.cpp */

	next_state = state;
	switch(state) {
	case MSGSTATE_HDR1:
	    header = data;
	    next_state = MSGSTATE_HDR2;
	    break;
	case MSGSTATE_HDR2:
	    header |= data<<8;
	    msg_length = header & LENGTH_MASK;
	    msg_type = header & TYPE_MASK;
	    //printf("%0x %d\n", msg_type, msg_length);
	    if (length == 2) {
		parse_message(msg_type, msg_length, msg_data);
		next_state = MSGSTATE_HDR1;
	    }
	    else {
		if (msg_length == 0x0)
		    length = 8192;
		else
		    length = msg_length - 2;
		n = 0;
		next_state = MSGSTATE_DATA;
	    }
	    break;
	case MSGSTATE_DATA:
	    msg_data[n++] = data;
	    length--;
	    if (length == 0) {
		parse_message(msg_type, msg_length, msg_data);
		next_state = MSGSTATE_HDR1;
	    }
	    break;
	}
	state = next_state;
    }

    printf("finished, c_msg = %d uc_msg = %d\n", c_msg, uc_msg);

    write_dongle(fd, run_state_stop, sizeof(run_state_stop));

    close(fd);
    if (fin != NULL) 
	fclose(fin);
    if (fout != NULL) 
	fclose(fout);
    fclose(f);

    return 0;
}
