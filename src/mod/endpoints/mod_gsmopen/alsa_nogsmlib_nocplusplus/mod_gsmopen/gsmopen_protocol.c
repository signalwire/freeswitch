#include "gsmopen.h"
//#include <iostream.h>

#ifndef NO_GSMLIB
#include <gsmlib/gsm_sms.h>
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#endif
#include <gsmlib/gsm_me_ta.h>
#include <iostream>


using namespace std;
using namespace gsmlib;
#endif// NO_GSMLIB

#ifdef ASTERISK
#define gsmopen_sleep usleep
#define gsmopen_strncpy strncpy
#define tech_pvt p
extern int gsmopen_debug;
extern char *gsmopen_console_active;
#else /* FREESWITCH */
#define gsmopen_sleep switch_sleep
#define gsmopen_strncpy switch_copy_string
extern switch_memory_pool_t *gsmopen_module_pool;
extern switch_endpoint_interface_t *gsmopen_endpoint_interface;
#endif /* ASTERISK */
//int samplerate_gsmopen = SAMPLERATE_GSMOPEN;

extern int running;
int gsmopen_dir_entry_extension = 1;

int option_debug = 100;


#ifdef WIN32
#define GSMLIBGIO
#else //WIN32
#undef GSMLIBGIO
#endif //WIN32

#ifdef WIN32
/***************/
// from http://www.openasthra.com/c-tidbits/gettimeofday-function-for-windows/

#include <time.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else /*  */
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif /*  */
struct sk_timezone {
	int tz_minuteswest;			/* minutes W of Greenwich */
	int tz_dsttime;				/* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct sk_timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;
	if (NULL != tv) {
		GetSystemTimeAsFileTime(&ft);
		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch */
		tmpres /= 10;			/*convert into microseconds */
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long) (tmpres / 1000000UL);
		tv->tv_usec = (long) (tmpres % 1000000UL);
	}
	if (NULL != tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return 0;
}

/***************/
#endif /* WIN32 */

#ifdef GSMOPEN_PORTAUDIO
#include "pablio.h"

#ifndef GIOVA48
#define SAMPLES_PER_FRAME 160
#else // GIOVA48
#define SAMPLES_PER_FRAME 960
#endif // GIOVA48

int gsmopen_portaudio_devlist(private_t *tech_pvt)
{
  int i, numDevices;
  const PaDeviceInfo *deviceInfo;

  numDevices = Pa_GetDeviceCount();
  if (numDevices < 0) {
    return 0;
  }
  for (i = 0; i < numDevices; i++) {
    deviceInfo = Pa_GetDeviceInfo(i);
    NOTICA
      ("Found PORTAUDIO device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
       GSMOPEN_P_LOG, i, deviceInfo->name, deviceInfo->maxInputChannels,
       deviceInfo->maxOutputChannels);
  }

  return numDevices;
}

int gsmopen_portaudio_init(private_t *tech_pvt)
{
  PaError err;
  int c;
  PaStreamParameters inputParameters, outputParameters;
  int numdevices;
  const PaDeviceInfo *deviceInfo;

#ifndef GIOVA48
  setenv("PA_ALSA_PLUGHW", "1", 1);
#endif // GIOVA48

  err = Pa_Initialize();
  if (err != paNoError)
    return err;

  numdevices = gsmopen_portaudio_devlist(tech_pvt);

  if (tech_pvt->portaudiocindex > (numdevices - 1)) {
    ERRORA("Portaudio Capture id=%d is out of range: valid id are from 0 to %d\n",
           GSMOPEN_P_LOG, tech_pvt->portaudiocindex, (numdevices - 1));
    return -1;
  }

  if (tech_pvt->portaudiopindex > (numdevices - 1)) {
    ERRORA("Portaudio Playback id=%d is out of range: valid id are from 0 to %d\n",
           GSMOPEN_P_LOG, tech_pvt->portaudiopindex, (numdevices - 1));
    return -1;
  }
  //inputParameters.device = 0;
  if (tech_pvt->portaudiocindex != -1) {
    inputParameters.device = tech_pvt->portaudiocindex;
  } else {
    inputParameters.device = Pa_GetDefaultInputDevice();
  }
  deviceInfo = Pa_GetDeviceInfo(inputParameters.device);
  NOTICA
    ("Using INPUT PORTAUDIO device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
     GSMOPEN_P_LOG, inputParameters.device, deviceInfo->name,
     deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
  if (deviceInfo->maxInputChannels == 0) {
    ERRORA
      ("No INPUT channels on device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
       GSMOPEN_P_LOG, inputParameters.device, deviceInfo->name,
       deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
    return -1;
  }
  inputParameters.channelCount = 1;
  inputParameters.sampleFormat = paInt16;
  //inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultHighInputLatency;
  inputParameters.suggestedLatency = 0.1;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  //outputParameters.device = 3;
  if (tech_pvt->portaudiopindex != -1) {
    outputParameters.device = tech_pvt->portaudiopindex;
  } else {
    outputParameters.device = Pa_GetDefaultOutputDevice();
  }
  deviceInfo = Pa_GetDeviceInfo(outputParameters.device);
  NOTICA
    ("Using OUTPUT PORTAUDIO device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
     GSMOPEN_P_LOG, outputParameters.device, deviceInfo->name,
     deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
  if (deviceInfo->maxOutputChannels == 0) {
    ERRORA
      ("No OUTPUT channels on device: id=%d\tname=%s\tmax input channels=%d\tmax output channels=%d\n",
       GSMOPEN_P_LOG, inputParameters.device, deviceInfo->name,
       deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
    return -1;
  }
#ifndef GIOVA48
  outputParameters.channelCount = 1;
#else // GIOVA48
  outputParameters.channelCount = 2;
#endif // GIOVA48
  outputParameters.sampleFormat = paInt16;
  //outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
  outputParameters.suggestedLatency = 0.1;
  outputParameters.hostApiSpecificStreamInfo = NULL;

/* build the pipe that will be polled on by pbx */
  c = pipe(tech_pvt->audiopipe);
  if (c) {
    ERRORA("Unable to create audio pipe\n", GSMOPEN_P_LOG);
    return -1;
  }
  fcntl(tech_pvt->audiopipe[0], F_SETFL, O_NONBLOCK);
  fcntl(tech_pvt->audiopipe[1], F_SETFL, O_NONBLOCK);

  err =
#ifndef GIOVA48
    OpenAudioStream(&tech_pvt->stream, &inputParameters, &outputParameters, 8000,
                    paClipOff|paDitherOff, SAMPLES_PER_FRAME, 0);
                    //&tech_pvt->speexecho, &tech_pvt->speexpreprocess, &tech_pvt->owner);

#else // GIOVA48
    OpenAudioStream(&tech_pvt->stream, &inputParameters, &outputParameters, 48000,
                    paDitherOff | paClipOff, SAMPLES_PER_FRAME, tech_pvt->audiopipe[1],
                    &tech_pvt->speexecho, &tech_pvt->speexpreprocess, &tech_pvt->owner);


#endif// GIOVA48
  if (err != paNoError) {
    ERRORA("Unable to open audio stream: %s\n", GSMOPEN_P_LOG, Pa_GetErrorText(err));
    return -1;
  }

/* the pipe is our audio fd for pbx to poll on */
  tech_pvt->gsmopen_sound_capt_fd = tech_pvt->audiopipe[0];

  return 0;
}
//int gsmopen_portaudio_write(private_t *tech_pvt, struct ast_frame *f)
int gsmopen_portaudio_write(private_t * tech_pvt, short *data, int datalen)
{
  int samples;
#ifdef GIOVA48
	//short buf[GSMOPEN_FRAME_SIZE * 2];
	short buf[3840];
	short *buf2;

    //ERRORA("1 f->datalen=: %d\n", GSMOPEN_P_LOG, f->datalen);




	memset(buf, '\0', GSMOPEN_FRAME_SIZE *2);

	buf2 = f->data;

	  int i=0, a=0;

	  for(i=0; i< f->datalen / sizeof(short); i++){
//stereo, 2 chan 48 -> mono 8
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  buf[a] = buf2[i];
		  a++;
		  /*
		  */
	  }
	  f->data = &buf;
	  f->datalen = f->datalen * 6;
    //ERRORA("2 f->datalen=: %d\n", GSMOPEN_P_LOG, f->datalen);
	  //f->datalen = f->datalen;
#endif // GIOVA48


  samples =
    WriteAudioStream(tech_pvt->stream, (short *) data, (int) (datalen / sizeof(short)), &tech_pvt->timer_write);

  if (samples != (int) (datalen / sizeof(short)))
    ERRORA("WriteAudioStream wrote: %d of %d\n", GSMOPEN_P_LOG, samples,
           (int) (datalen / sizeof(short)));

  return samples;
}
//struct ast_frame *gsmopen_portaudio_read(private_t *tech_pvt)
#define AST_FRIENDLY_OFFSET 0
int gsmopen_portaudio_read(private_t * tech_pvt, short *data, int datalen)
{
#if 0
  //static struct ast_frame f;
  static short __buf[GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
  short *buf;
  static short __buf2[GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
  short *buf2;
  int samples;
  //char c;

  memset(__buf, '\0', (GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2));

  buf = __buf + AST_FRIENDLY_OFFSET / 2;

  memset(__buf2, '\0', (GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2));

  buf2 = __buf2 + AST_FRIENDLY_OFFSET / 2;

#if 0
  f.frametype = AST_FRAME_NULL;
  f.subclass = 0;
  f.samples = 0;
  f.datalen = 0;

#ifdef ASTERISK_VERSION_1_6_1
  f.data.ptr = NULL;
#else
  f.data = NULL;
#endif /* ASTERISK_VERSION_1_6_1 */
  f.offset = 0;
  f.src = gsmopen_type;
  f.mallocd = 0;
  f.delivery.tv_sec = 0;
  f.delivery.tv_usec = 0;
#endif //0

  //if ((samples = ReadAudioStream(tech_pvt->stream, buf, SAMPLES_PER_FRAME)) == 0) 
  //if ((samples = ReadAudioStream(tech_pvt->stream, data, datalen/sizeof(short))) == 0) 
  if (samples = ReadAudioStream(tech_pvt->stream, (short *)data, datalen, &tech_pvt->timer_read) == 0) {
    //do nothing
  } else {
#ifdef GIOVA48
	  int i=0, a=0;

	  samples = samples / 6;
	  for(i=0; i< samples; i++){
		  buf2[i] = buf[a];
		  a = a + 6; //mono, 1 chan 48 -> 8
	  }
	  buf = buf2;

#if 0
    /* A real frame */
    f.frametype = AST_FRAME_VOICE;
    f.subclass = AST_FORMAT_SLINEAR;
    f.samples = GSMOPEN_FRAME_SIZE/6;
    f.datalen = GSMOPEN_FRAME_SIZE * 2/6;
#endif //0
#else// GIOVA48
#if 0
    /* A real frame */
    f.frametype = AST_FRAME_VOICE;
    f.subclass = AST_FORMAT_SLINEAR;
    f.samples = GSMOPEN_FRAME_SIZE;
    f.datalen = GSMOPEN_FRAME_SIZE * 2;
#endif //0
#endif// GIOVA48

#if 0
#ifdef ASTERISK_VERSION_1_6_1
    f.data.ptr = buf;
#else
    f.data = buf;
#endif /* ASTERISK_VERSION_1_6_1 */
    f.offset = AST_FRIENDLY_OFFSET;
    f.src = gsmopen_type;
    f.mallocd = 0;
#endif //0
  }

#if 0
  read(tech_pvt->audiopipe[0], &c, 1);

  return &f;
#endif //0
#endif //0

  int samples;
  samples = ReadAudioStream(tech_pvt->stream, (short *)data, datalen, &tech_pvt->timer_read);
    //WARNINGA("samples=%d\n", GSMOPEN_P_LOG, samples);

  return samples;
}
int gsmopen_portaudio_shutdown(private_t *tech_pvt)
{
  PaError err;

  err = CloseAudioStream(tech_pvt->stream);

  if (err != paNoError)
    ERRORA("not able to CloseAudioStream\n", GSMOPEN_P_LOG);

  Pa_Terminate();
  return 0;
}




#endif // GSMOPEN_PORTAUDIO
#ifndef GSMLIBGIO
int gsmopen_serial_init(private_t * tech_pvt, speed_t controldevice_speed)
{
	int fd;
	int rt;
	struct termios tp;
	unsigned int status = 0;
	unsigned int flags = TIOCM_DTR;

/* if there is a file descriptor, close it. But it is probably just an old value, so don't check for close success*/
	fd = tech_pvt->controldevfd;
	if (fd) {
		close(fd);
	}
/*  open the serial port */
//#ifdef __CYGWIN__
	fd = open(tech_pvt->controldevice_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	sleep(1);
	close(fd);
//#endif /* __CYGWIN__ */
	fd = open(tech_pvt->controldevice_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd == -1) {
		perror("open error ");
		DEBUGA_GSMOPEN("serial error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		tech_pvt->controldevfd = fd;
		return -1;
	}
/*  flush it */
	rt = tcflush(fd, TCIFLUSH);
	if (rt == -1) {
		ERRORA("serial error: %s", GSMOPEN_P_LOG, strerror(errno));
	}
/*  attributes */
	tp.c_cflag = B0 | CS8 | CLOCAL | CREAD | HUPCL;
	tp.c_iflag = IGNPAR;
	tp.c_cflag &= ~CRTSCTS;
	tp.c_oflag = 0;
	tp.c_lflag = 0;
	tp.c_cc[VMIN] = 1;
	tp.c_cc[VTIME] = 0;
/*  set controldevice_speed */
	rt = cfsetispeed(&tp, tech_pvt->controldevice_speed);
	if (rt == -1) {
		ERRORA("serial error: %s, speed was: %d", GSMOPEN_P_LOG, strerror(errno), tech_pvt->controldevice_speed);
	}
	rt = cfsetospeed(&tp, tech_pvt->controldevice_speed);
	if (rt == -1) {
		ERRORA("serial error: %s", GSMOPEN_P_LOG, strerror(errno));
	}
/*  set port attributes */
	if (tcsetattr(fd, TCSADRAIN, &tp) == -1) {
		ERRORA("serial error: %s", GSMOPEN_P_LOG, strerror(errno));
	}
	rt = tcsetattr(fd, TCSANOW, &tp);
	if (rt == -1) {
		ERRORA("serial error: %s", GSMOPEN_P_LOG, strerror(errno));
	}
#ifndef __CYGWIN__
	ioctl(fd, TIOCMGET, &status);
	status |= TIOCM_DTR;		/*  Set DTR high */
	status &= ~TIOCM_RTS;		/*  Set RTS low */
	ioctl(fd, TIOCMSET, &status);
	ioctl(fd, TIOCMGET, &status);
	ioctl(fd, TIOCMBIS, &flags);
	flags = TIOCM_RTS;
	ioctl(fd, TIOCMBIC, &flags);
	ioctl(fd, TIOCMGET, &status);
#else /* __CYGWIN__ */
	ioctl(fd, TIOCMGET, &status);
	status |= TIOCM_DTR;		/*  Set DTR high */
	status &= ~TIOCM_RTS;		/*  Set RTS low */
	ioctl(fd, TIOCMSET, &status);
#endif /* __CYGWIN__ */
	tech_pvt->controldevfd = fd;
	DEBUGA_GSMOPEN("Syncing Serial, fd=%d, protocol=%d\n", GSMOPEN_P_LOG, fd, tech_pvt->controldevprotocol);
	rt = gsmopen_serial_sync(tech_pvt);
	if (rt == -1) {
		ERRORA("Serial init error\n", GSMOPEN_P_LOG);
		return -1;
	}
	return (fd);
}
#else //GSMLIBGIO
#ifdef WIN32
int gsmopen_serial_init(private_t * tech_pvt, int controldevice_speed)
#else 
int gsmopen_serial_init(private_t * tech_pvt, speed_t controldevice_speed)
#endif //WIN32
{
	int i;
	string ciapa;
	SMSMessageRef sms;
	char content2[1000];
	int size;

#ifdef WIN32
	Ref <Port> port = new Win32SerialPort((string) tech_pvt->controldevice_name, 38400);
#else
	//Ref<Port> port = new UnixSerialPort((string)argv[1], B38400);
	Ref < Port > port = new UnixSerialPort((string) tech_pvt->controldevice_name, B115200);
#endif
	MeTa m(port);

	//cout << "Creating GsmAt object" << endl;
	Ref <GsmAt> gsmat = new GsmAt(m);

	//cout << "Using GsmAt object" << endl;
	//cout << gsmat->chat("AT", "OK", false, false) << endl;
	//cout << gsmat->chat("D3472665618;") << endl;
	gsmat->putLine("AT+cgmm", true);
	for (i = 0; i < 4; i++) {
		ciapa = gsmat->getLine();
		//cout << "PRESO: |||" << ciapa << "|||" << endl;
		NOTICA("PRESO %d |||%s|||\n", GSMOPEN_P_LOG, i, ciapa.c_str());
		//gsmopen_sleep(5000);
	}

		sms = SMSMessage::decode("079194710167120004038571F1390099406180904480A0D41631067296EF7390383D07CD622E58CD95CB81D6EF39BDEC66BFE7207A794E2FBB4320AFB82C07E56020A8FC7D9687DBED32285C9F83A06F769A9E5EB340D7B49C3E1FA3C3663A0B24E4CBE76516680A7FCBE920725A5E5ED341F0B21C346D4E41E1BA790E4286DDE4BC0BD42CA3E5207258EE1797E5A0BA9B5E9683C86539685997EBEF61341B249BC966"); // dataCodingScheme = 0
		NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());
		sms = SMSMessage::decode("0791934329002000040C9193432766658100009001211133318004D4F29C0E"); // dataCodingScheme = 0
		NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());
		sms = SMSMessage::decode("0791934329002000040C919343276665810008900121612521801600CC00E800E900F900F200E00020006300690061006F"); // dataCodingScheme = 8
		NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());
		sms = SMSMessage::decode("0791934329002000040C919343276665810008900172002293404C006300690061006F0020003100320033002000620065006C00E80020043D043E0432043E044104420438002005DC05E7002005E805D005EA0020FE8EFEE0FEA0FEE4FECBFE9300204EBA5927");	// dataCodingScheme = 8 , text=ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大
		NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());
		sms = SMSMessage::decode("07911497941902F00414D0E474989D769F5DE4320839001040122151820000"); // dataCodingScheme = 0
		NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());

#if 0
            size = MultiByteToWideChar(CP_OEMCP, 0, username, strlen(username)+1, UserName, 0);
            UserName=(wchar_t*)GlobalAlloc(GME­ M_ZEROINIT, size);
            ret = MultiByteToWideChar(CP_OEMCP, 0, username, strlen(username)+1, UserName, size);
            if(ret == 0)
                getError(GetLastError());
#endif //0
	return (-1);
}

#endif //GSMLIBGIO


int gsmopen_serial_read(private_t * tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_read_AT(tech_pvt, 0, 100000, 0, NULL, 1);	// a 10th of a second timeout
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_read_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_read_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */
	return -1;
}


int gsmopen_serial_sync(private_t * tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_sync_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_sync_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_sync_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */

	return -1;
}

int gsmopen_serial_config(private_t * tech_pvt)
{
#ifndef NO_GSMLIB
		SMSMessageRef sms;
		char content2[1000];
		//sms = SMSMessage::decode("079194710167120004038571F1390099406180904480A0D41631067296EF7390383D07CD622E58CD95CB81D6EF39BDEC66BFE7207A794E2FBB4320AFB82C07E56020A8FC7D9687DBED32285C9F83A06F769A9E5EB340D7B49C3E1FA3C3663A0B24E4CBE76516680A7FCBE920725A5E5ED341F0B21C346D4E41E1BA790E4286DDE4BC0BD42CA3E5207258EE1797E5A0BA9B5E9683C86539685997EBEF61341B249BC966"); // dataCodingScheme = 0
		//sms = SMSMessage::decode("0791934329002000040C9193432766658100009001211133318004D4F29C0E"); // dataCodingScheme = 0
		//sms = SMSMessage::decode("0791934329002000040C919343276665810008900121612521801600CC00E800E900F900F200E00020006300690061006F"); // dataCodingScheme = 8
		sms = SMSMessage::decode("0791934329002000040C919343276665810008900172002293404C006300690061006F0020003100320033002000620065006C00E80020043D043E0432043E044104420438002005DC05E7002005E805D005EA0020FE8EFEE0FEA0FEE4FECBFE9300204EBA5927");	// dataCodingScheme = 8 , text=ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大
		//sms = SMSMessage::decode("07911497941902F00414D0E474989D769F5DE4320839001040122151820000"); // dataCodingScheme = 0
		//NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());

		memset(content2, '\0', sizeof(content2));
		if (sms->dataCodingScheme().getAlphabet() == DCS_DEFAULT_ALPHABET) {
			iso_8859_1_to_utf8(tech_pvt, (char *) sms->userData().c_str(), content2, sizeof(content2));
		} else if (sms->dataCodingScheme().getAlphabet() == DCS_SIXTEEN_BIT_ALPHABET) {
			ucs2_to_utf8(tech_pvt, (char *) bufToHex((unsigned char *) sms->userData().data(), sms->userData().length()).c_str(), content2,
					sizeof(content2));
		} else {
			ERRORA("dataCodingScheme not supported=%d\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());

		}
		//NOTICA("dataCodingScheme=%d\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());
		//NOTICA("userData= |||%s|||\n", GSMOPEN_P_LOG, content2);
#endif// NO_GSMLIB

	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_config_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_config_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_config_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */

	return -1;
}

int gsmopen_serial_config_AT(private_t * tech_pvt)
{
	int res;
	char at_command[5];
	int i;

/* initial_pause? */
	if (tech_pvt->at_initial_pause) {
		DEBUGA_GSMOPEN("sleeping for %d usec\n", GSMOPEN_P_LOG, tech_pvt->at_initial_pause);
		gsmopen_sleep(tech_pvt->at_initial_pause);
	}

/* go until first empty preinit string, or last preinit string */
	while (1) {

		if (strlen(tech_pvt->at_preinit_1)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_1, tech_pvt->at_preinit_1_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_1, tech_pvt->at_preinit_1_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_2)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_2, tech_pvt->at_preinit_2_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_2, tech_pvt->at_preinit_2_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_3)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_3, tech_pvt->at_preinit_3_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_3, tech_pvt->at_preinit_3_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_4)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_4, tech_pvt->at_preinit_4_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_4, tech_pvt->at_preinit_4_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_preinit_5)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_preinit_5, tech_pvt->at_preinit_5_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_preinit_5, tech_pvt->at_preinit_5_expect);
			}
		} else {
			break;
		}

		break;
	}

/* after_preinit_pause? */
	if (tech_pvt->at_after_preinit_pause) {
		DEBUGA_GSMOPEN("sleeping for %d usec\n", GSMOPEN_P_LOG, tech_pvt->at_after_preinit_pause);
		gsmopen_sleep(tech_pvt->at_after_preinit_pause);
	}

	/* phone, brother, art you alive? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT");
	if (res) {
		ERRORA("no response to AT\n", GSMOPEN_P_LOG);
		return -1;
	}
	/* for motorola, bring it back to "normal" mode if it happens to be in another mode */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+mode=0");
	if (res) {
		DEBUGA_GSMOPEN("AT+mode=0 does not get OK from the phone. If it is NOT Motorola," " no problem.\n", GSMOPEN_P_LOG);
	}
	gsmopen_sleep(50000);
	/* for motorola end */

	/* reset AT configuration to phone default */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "ATZ");
	if (res) {
		DEBUGA_GSMOPEN("ATZ failed\n", GSMOPEN_P_LOG);
	}

	/* disable AT command echo */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "ATE0");
	if (res) {
		DEBUGA_GSMOPEN("ATE0 failed\n", GSMOPEN_P_LOG);
	}

	/* disable extended error reporting */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMEE=0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMEE failed\n", GSMOPEN_P_LOG);
	}

	/* various phone manufacturer identifier */
	for (i = 0; i < 10; i++) {
		memset(at_command, 0, sizeof(at_command));
		sprintf(at_command, "ATI%d", i);
		res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		if (res) {
			DEBUGA_GSMOPEN("ATI%d command failed, continue\n", GSMOPEN_P_LOG, i);
		}
	}

	/* phone manufacturer */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CGMI");
	if (res) {
		DEBUGA_GSMOPEN("AT+CGMI failed\n", GSMOPEN_P_LOG);
	}

	/* phone model */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CGMM");
	if (res) {
		DEBUGA_GSMOPEN("AT+CGMM failed\n", GSMOPEN_P_LOG);
	}

	/* signal network registration with a +CREG unsolicited msg */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CREG=1");
	if (res) {
		DEBUGA_GSMOPEN("AT+CREG=1 failed\n", GSMOPEN_P_LOG);
		tech_pvt->network_creg_not_supported = 1;
	}
	if(!tech_pvt->network_creg_not_supported){
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CREG?");
		if (res) {
			DEBUGA_GSMOPEN("AT+CREG? failed\n", GSMOPEN_P_LOG);
		}
	}
	/* query signal strength */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSQ");
	if (res) {
		DEBUGA_GSMOPEN("AT+CSQ failed\n", GSMOPEN_P_LOG);
	}
	/* IMEI */
	tech_pvt->requesting_imei = 1;
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+GSN");
	tech_pvt->requesting_imei = 0;
	if (res) {
		DEBUGA_GSMOPEN("AT+GSN failed\n", GSMOPEN_P_LOG);
		tech_pvt->requesting_imei = 1;
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CGSN");
		tech_pvt->requesting_imei = 0;
		if (res) {
			DEBUGA_GSMOPEN("AT+CGSN failed\n", GSMOPEN_P_LOG);
		}
	}
	/* IMSI */
	tech_pvt->requesting_imsi = 1;
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CIMI");
	tech_pvt->requesting_imsi = 0;
	if (res) {
		DEBUGA_GSMOPEN("AT+CIMI failed\n", GSMOPEN_P_LOG);
	}

	/* signal incoming SMS with a +CMTI unsolicited msg */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CNMI=3,1,0,0,0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CNMI=3,1,0,0,0 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->sms_cnmi_not_supported = 1;
		tech_pvt->gsmopen_serial_sync_period = 30;	//FIXME in config
	}
	/* what is the Message Center address (number) to which the SMS has to be sent? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCA?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CSCA? failed, continue\n", GSMOPEN_P_LOG);
	}
	/* what is the Message Format of SMSs? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMGF? failed, continue\n", GSMOPEN_P_LOG);
	}
#ifdef NO_GSMLIB
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=1");
	if (res) {
		ERRORA("Error setting SMS sending mode to TEXT on the cellphone, let's hope is TEXT by default. Continuing\n", GSMOPEN_P_LOG);
	}
	tech_pvt->sms_pdu_not_supported = 1;
#else // NO_GSMLIB
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=0");
	if (res) {
		WARNINGA("Error setting SMS sending mode to PDU on the cellphone, falling back to TEXT mode. Continuing\n", GSMOPEN_P_LOG);
		tech_pvt->sms_pdu_not_supported = 1;
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=1");
		if (res) {
			ERRORA("Error setting SMS sending mode to TEXT on the cellphone, let's hope is TEXT by default. Continuing\n", GSMOPEN_P_LOG);
		}
	}
#endif // NO_GSMLIB
	/* what is the Charset of SMSs? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CSCS? failed, continue\n", GSMOPEN_P_LOG);
	}

	tech_pvt->no_ucs2 = 0;
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"UCS2\"");
	if (res) {
		WARNINGA("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone, let's try with 'GSM'\n", GSMOPEN_P_LOG);
		tech_pvt->no_ucs2 = 1;
	}

	if (tech_pvt->no_ucs2) {
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"GSM\"");
		if (res) {
			WARNINGA("AT+CSCS=\"GSM\" (set TE messages to GSM)  do not got OK from the phone\n", GSMOPEN_P_LOG);
		}
		//res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSMP=17,167,0,16"); //"flash", class 0  sms 7 bit
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSMP=17,167,0,0");	//normal, 7 bit message
		if (res) {
			WARNINGA("AT+CSMP do not got OK from the phone, continuing\n", GSMOPEN_P_LOG);
		}
	} else {
		//res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSMP=17,167,0,20"); //"flash", class 0 sms 16 bit unicode
		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSMP=17,167,0,8");	//unicode, 16 bit message
		if (res) {
			WARNINGA("AT+CSMP do not got OK from the phone, continuing\n", GSMOPEN_P_LOG);
		}
	}

	/* is the unsolicited reporting of mobile equipment event supported? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMER=?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMER=? failed, continue\n", GSMOPEN_P_LOG);
	}
	/* request unsolicited reporting of mobile equipment indicators' events, to be screened by categories reported by +CIND=? */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMER=3,0,0,1");
	if (res) {
		DEBUGA_GSMOPEN("AT+CMER=? failed, continue\n", GSMOPEN_P_LOG);
	}

	/* is the solicited reporting of mobile equipment indications supported? */

	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CIND=?");
	if (res) {
		DEBUGA_GSMOPEN("AT+CIND=? failed, continue\n", GSMOPEN_P_LOG);
	}

	/* is the unsolicited reporting of call monitoring supported? sony-ericsson specific */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT*ECAM=?");
	if (res) {
		DEBUGA_GSMOPEN("AT*ECAM=? failed, continue\n", GSMOPEN_P_LOG);
	}
	/* enable the unsolicited reporting of call monitoring. sony-ericsson specific */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT*ECAM=1");
	if (res) {
		DEBUGA_GSMOPEN("AT*ECAM=1 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->at_has_ecam = 0;
	} else {
		tech_pvt->at_has_ecam = 1;
	}

	/* disable unsolicited signaling of call list */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CLCC=0");
	if (res) {
		DEBUGA_GSMOPEN("AT+CLCC=0 failed, continue\n", GSMOPEN_P_LOG);
		tech_pvt->at_has_clcc = 0;
	} else {
		tech_pvt->at_has_clcc = 1;
	}

	/* give unsolicited caller id when incoming call */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CLIP=1");
	if (res) {
		DEBUGA_GSMOPEN("AT+CLIP failed, continue\n", GSMOPEN_P_LOG);
	}
	/* for motorola */
	res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+MCST=1");	/* motorola call control codes
																   (to know when call is disconnected (they
																   don't give you "no carrier") */
	if (res) {
		DEBUGA_GSMOPEN("AT+MCST=1 does not get OK from the phone. If it is NOT Motorola," " no problem.\n", GSMOPEN_P_LOG);
	}
	/* for motorola end */

/* go until first empty postinit string, or last postinit string */
	while (1) {

		if (strlen(tech_pvt->at_postinit_1)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_1, tech_pvt->at_postinit_1_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_1, tech_pvt->at_postinit_1_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_2)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_2, tech_pvt->at_postinit_2_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_2, tech_pvt->at_postinit_2_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_3)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_3, tech_pvt->at_postinit_3_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_3, tech_pvt->at_postinit_3_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_4)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_4, tech_pvt->at_postinit_4_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_4, tech_pvt->at_postinit_4_expect);
			}
		} else {
			break;
		}

		if (strlen(tech_pvt->at_postinit_5)) {
			res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_postinit_5, tech_pvt->at_postinit_5_expect);
			if (res) {
				DEBUGA_GSMOPEN("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, tech_pvt->at_postinit_5, tech_pvt->at_postinit_5_expect);
			}
		} else {
			break;
		}

		break;
	}

	return 0;
}


int gsmopen_serial_sync_AT(private_t * tech_pvt)
{
	gsmopen_sleep(10000);				/* 10msec */
	time(&tech_pvt->gsmopen_serial_synced_timestamp);
	return 0;
}
int gsmopen_serial_read_AT(private_t * tech_pvt, int look_for_ack, int timeout_usec, int timeout_sec, const char *expected_string, int expect_crlf)
{
	int select_err = 1;
	int res;
	fd_set read_fds;
	struct timeval timeout;
	char tmp_answer[AT_BUFSIZ];
	char tmp_answer2[AT_BUFSIZ];
	char *tmp_answer_ptr;
	char *last_line_ptr;
	int i = 0;
	int read_count = 0;
	int la_counter = 0;
	int at_ack = -1;
	int la_read = 0;

	if(!running || !tech_pvt->running){
		return -1;
	}

	FD_ZERO(&read_fds);
	FD_SET(tech_pvt->controldevfd, &read_fds);

	//NOTICA (" INSIDE this gsmopen_serial_device %s \n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
	tmp_answer_ptr = tmp_answer;
	memset(tmp_answer, 0, sizeof(char) * AT_BUFSIZ);

	timeout.tv_sec = timeout_sec;
	timeout.tv_usec = timeout_usec;
	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);

	while ((!tech_pvt->controldev_dead) && ((select_err = select(tech_pvt->controldevfd + 1, &read_fds, NULL, NULL, &timeout)) > 0)) {
		char *token_ptr;
		timeout.tv_sec = timeout_sec;	//reset the timeout, linux modify it
		timeout.tv_usec = timeout_usec;	//reset the timeout, linux modify it
		read_count = read(tech_pvt->controldevfd, tmp_answer_ptr, AT_BUFSIZ - (tmp_answer_ptr - tmp_answer));

		if (read_count == 0) {
			ERRORA
				("read 0 bytes!!! Nenormalno! Marking this gsmopen_serial_device %s as dead, andif it is owned by a channel, hanging up. Maybe the phone is stuck, switched off, power down or battery exhausted\n",
				 GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->controldev_dead = 1;
			close(tech_pvt->controldevfd);
			ERRORA("gsmopen_serial_monitor failed, declaring %s dead\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
			tech_pvt->running=0;
			alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "gsmopen_serial_monitor failed, declaring interface dead");
			tech_pvt->active=0;
			tech_pvt->name[0]='\0';

			UNLOCKA(tech_pvt->controldev_lock);
			if (tech_pvt->owner) {
				tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
				gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
			}
			switch_sleep(1000000);
			return -1;
		}

		if (option_debug > 90) {
			//DEBUGA_GSMOPEN("1 read %d bytes, --|%s|--\n", GSMOPEN_P_LOG, read_count, tmp_answer_ptr);
			//DEBUGA_GSMOPEN("2 read %d bytes, --|%s|--\n", GSMOPEN_P_LOG, read_count, tmp_answer);
		}
		tmp_answer_ptr = tmp_answer_ptr + read_count;


		la_counter = 0;
		memset(tmp_answer2, 0, sizeof(char) * AT_BUFSIZ);
		strcpy(tmp_answer2, tmp_answer);
		if ((token_ptr = strtok(tmp_answer2, "\n\r"))) {
			last_line_ptr = token_ptr;
			strncpy(tech_pvt->line_array.result[la_counter], token_ptr, AT_MESG_MAX_LENGTH);
			if (strlen(token_ptr) > AT_MESG_MAX_LENGTH) {
				WARNINGA
					("AT mesg longer than buffer, original message was: |%s|, in buffer only: |%s|\n",
					 GSMOPEN_P_LOG, token_ptr, tech_pvt->line_array.result[la_counter]);
			}
			la_counter++;
			while ((token_ptr = strtok(NULL, "\n\r"))) {
				last_line_ptr = token_ptr;
				strncpy(tech_pvt->line_array.result[la_counter], token_ptr, AT_MESG_MAX_LENGTH);
				if (strlen(token_ptr) > AT_MESG_MAX_LENGTH) {
					WARNINGA
						("AT mesg longer than buffer, original message was: |%s|, in buffer only: |%s|\n",
						 GSMOPEN_P_LOG, token_ptr, tech_pvt->line_array.result[la_counter]);
				}
				la_counter++;
			}
		} else {
			last_line_ptr = tmp_answer;
		}

		if (expected_string && !expect_crlf) {
			DEBUGA_GSMOPEN
				("last_line_ptr=|%s|, expected_string=|%s|, expect_crlf=%d, memcmp(last_line_ptr, expected_string, strlen(expected_string)) = %d\n",
				 GSMOPEN_P_LOG, last_line_ptr, expected_string, expect_crlf, memcmp(last_line_ptr, expected_string, strlen(expected_string)));
		}

		if (expected_string && !expect_crlf && !memcmp(last_line_ptr, expected_string, strlen(expected_string))
			) {
			strncpy(tech_pvt->line_array.result[la_counter], last_line_ptr, AT_MESG_MAX_LENGTH);
			// match expected string -> accept it withtout CRLF
			la_counter++;

		}
		/* if the last line read was not a complete line, we'll read the rest in the future */
		else if (tmp_answer[strlen(tmp_answer) - 1] != '\r' && tmp_answer[strlen(tmp_answer) - 1] != '\n')
			la_counter--;

		/* let's list the complete lines read so far, without re-listing the lines that has yet been listed */
		if (option_debug > 1) {
			for (i = la_read; i < la_counter; i++)
				DEBUGA_GSMOPEN("Read line %d: |%s|\n", GSMOPEN_P_LOG, i, tech_pvt->line_array.result[i]);
		}

		/* let's interpret the complete lines read so far (WITHOUT looking for OK, ERROR, and EXPECTED_STRING), without re-interpreting the lines that has been yet interpreted, so we're sure we don't miss anything */
		for (i = la_read; i < la_counter; i++) {

			if ((strcmp(tech_pvt->line_array.result[i], "RING") == 0)) {
				/* with first RING we wait for callid */
				gettimeofday(&(tech_pvt->ringtime), NULL);
				/* give CALLID (+CLIP) a chance, wait for the next RING before answering */
				if (tech_pvt->phone_callflow == CALLFLOW_INCOMING_RING) {
					/* we're at the second ring, set the interface state, will be answered by gsmopen_do_monitor */
					DEBUGA_GSMOPEN("|%s| got second RING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					tech_pvt->interface_state = GSMOPEN_STATE_RING;
				} else {
					/* we're at the first ring, so there is no CALLID yet thus clean the previous one 
					   just in case we don't receive the caller identification in this new call */
					memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
					memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));
					/* only send AT+CLCC? if the device previously reported its support */
					if (tech_pvt->at_has_clcc != 0) {
						/* we're at the first ring, try to get CALLID (with +CLCC) */
						DEBUGA_GSMOPEN("|%s| got first RING, sending AT+CLCC?\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						res = gsmopen_serial_write_AT_noack(tech_pvt, "AT+CLCC?");
						if (res) {
							ERRORA("AT+CLCC? (call list) was not correctly sent to the phone\n", GSMOPEN_P_LOG);
						}
					} else {
						DEBUGA_GSMOPEN("|%s| got first RING, but not sending AT+CLCC? as this device "
									   "seems not to support\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					}
				}
				tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CLCC", 5) == 0)) {
				int commacount = 0;
				int a = 0;
				int b = 0;
				int c = 0;
				/* with clcc we wait for clip */
				memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
				memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));

				for (a = 0; a < strlen(tech_pvt->line_array.result[i]); a++) {

					if (tech_pvt->line_array.result[i][a] == ',') {
						commacount++;
					}
					if (commacount == 5) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_number[b] = tech_pvt->line_array.result[i][a];
							b++;
						}
					}
					if (commacount == 7) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_name[c] = tech_pvt->line_array.result[i][a];
							c++;
						}
					}
				}

				tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;
				DEBUGA_GSMOPEN("|%s| CLCC CALLID: name is %s, number is %s\n", GSMOPEN_P_LOG,
							   tech_pvt->line_array.result[i],
							   tech_pvt->callid_name[0] ? tech_pvt->callid_name : "not available",
							   tech_pvt->callid_number[0] ? tech_pvt->callid_number : "not available");
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CLIP", 5) == 0)) {
				int commacount = 0;
				int a = 0;
				int b = 0;
				int c = 0;
				/* with CLIP, we want to answer right away */
				memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
				memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));


				for (a = 7; a < strlen(tech_pvt->line_array.result[i]); a++) {
					if (tech_pvt->line_array.result[i][a] == ',') {
						commacount++;
					}
					if (commacount == 0) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_number[b] = tech_pvt->line_array.result[i][a];
							b++;
						}
					}
					if (commacount == 4) {
						if (tech_pvt->line_array.result[i][a] != ',' && tech_pvt->line_array.result[i][a] != '"') {
							tech_pvt->callid_name[c] = tech_pvt->line_array.result[i][a];
							c++;
						}
					}
				}

				if (tech_pvt->interface_state != GSMOPEN_STATE_RING) {
					gettimeofday(&(tech_pvt->call_incoming_time), NULL);
					DEBUGA_GSMOPEN("GSMOPEN_STATE_RING call_incoming_time.tv_sec=%ld\n", GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec);

				}

				tech_pvt->interface_state = GSMOPEN_STATE_RING;
				tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;
				DEBUGA_GSMOPEN("|%s| CLIP INCOMING CALLID: name is %s, number is %s\n", GSMOPEN_P_LOG,
							   tech_pvt->line_array.result[i],
							   (strlen(tech_pvt->callid_name) && tech_pvt->callid_name[0] != 1) ? tech_pvt->callid_name : "not available",
							   strlen(tech_pvt->callid_number) ? tech_pvt->callid_number : "not available");

				if (!strlen(tech_pvt->callid_number)) {
					strcpy(tech_pvt->callid_number, "not available");
				}

				if (!strlen(tech_pvt->callid_name) && tech_pvt->callid_name[0] != 1) {
					strncpy(tech_pvt->callid_name, tech_pvt->callid_number, sizeof(tech_pvt->callid_name));
					//strncpy(tech_pvt->callid_name, tech_pvt->callid_number, sizeof(tech_pvt->callid_name)) ;
					snprintf(tech_pvt->callid_name, sizeof(tech_pvt->callid_name), "GSMopen: %s", tech_pvt->callid_number);
				}

				DEBUGA_GSMOPEN("|%s| CLIP INCOMING CALLID: NOW name is %s, number is %s\n", GSMOPEN_P_LOG,
							   tech_pvt->line_array.result[i], tech_pvt->callid_name, tech_pvt->callid_number);
			}

			if ((strcmp(tech_pvt->line_array.result[i], "BUSY") == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_LINEBUSY;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_LINEBUSY\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				//if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner && tech_pvt->phone_callflow != CALLFLOW_CALL_DOWN) {
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->phone_callflow != CALLFLOW_CALL_DOWN) {
					//ast_setstate(tech_pvt->owner, GSMOPEN_STATE_BUSY);
					//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_BUSY);
					//cicopet
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						//gsmopen_hangup(tech_pvt);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}
					//
					//tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);

				} else {
					ERRORA("Why BUSY now?\n", GSMOPEN_P_LOG);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "NO ANSWER") == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOANSWER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOANSWER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NO_ANSWER;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				} else {
					ERRORA("Why NO ANSWER now?\n", GSMOPEN_P_LOG);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "NO CARRIER") == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOCARRIER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOCARRIER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
					//cicopet
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						//gsmopen_hangup(tech_pvt);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}
					//
					//tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				} else {
					ERRORA("Why NO CARRIER now?\n", GSMOPEN_P_LOG);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CBC:", 5) == 0)) {
				int power_supply, battery_strenght, err;

				power_supply = battery_strenght = 0;

				err = sscanf(&tech_pvt->line_array.result[i][6], "%d,%d", &power_supply, &battery_strenght);
				if (err < 2) {
					DEBUGA_GSMOPEN("|%s| is not formatted as: |+CBC: xx,yy| now trying  |+CBC:xx,yy|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

					err = sscanf(&tech_pvt->line_array.result[i][5], "%d,%d", &power_supply, &battery_strenght);
					DEBUGA_GSMOPEN("|%s| +CBC: Powered by %s, battery strenght=%d\n", GSMOPEN_P_LOG,
								   tech_pvt->line_array.result[i], power_supply ? "power supply" : "battery", battery_strenght);

				}

				if (err < 2) {
					DEBUGA_GSMOPEN("|%s| is not formatted as: |+CBC:xx,yy| giving up\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}

				else {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| +CBC: Powered by %s, battery strenght=%d\n", GSMOPEN_P_LOG,
									   tech_pvt->line_array.result[i], power_supply ? "power supply" : "battery", battery_strenght);
					if (!power_supply) {
						if (battery_strenght < 10) {
							ERRORA("|%s| BATTERY ALMOST EXHAUSTED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						} else if (battery_strenght < 20) {
							WARNINGA("|%s| BATTERY LOW\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

						}

					}
				}

			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CSQ:", 5) == 0)) {
				int signal_quality, ber, err;

				signal_quality = ber = 0;

				err = sscanf(&tech_pvt->line_array.result[i][6], "%d,%d", &signal_quality, &ber);
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CSQ: Signal Quality: %d, Error Rate=%d\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], signal_quality, ber);
				if (err < 2) {
					ERRORA("|%s| is not formatted as: |+CSQ: xx,yy|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				} else {
					if (signal_quality < 11 || signal_quality == 99) {
						ERRORA
							("|%s| CELLPHONE GETS ALMOST NO SIGNAL, consider to move it or additional antenna\n",
							 GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->got_signal=0;
				alarm_event(tech_pvt, ALARM_NETWORK_NO_SIGNAL, "CELLPHONE GETS ALMOST NO SIGNAL, consider to move it or additional antenna");
					} else if (signal_quality < 15) {
						WARNINGA("|%s| CELLPHONE GETS SIGNAL LOW\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						tech_pvt->got_signal=1;
				alarm_event(tech_pvt, ALARM_NETWORK_LOW_SIGNAL, "CELLPHONE GETS SIGNAL LOW");
					} else {
						tech_pvt->got_signal=2;
					}

				}

			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CREG:", 6) == 0)) {
				int n, stat, err;

				n = stat = 0;

				err = sscanf(&tech_pvt->line_array.result[i][6], "%d,%d", &n, &stat);
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CREG: Display: %d, Registration=%d\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], n, stat);
				if (err < 2) {
					WARNINGA("|%s| is not formatted as: |+CREG: xx,yy|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				} 
				if (stat==0) {
					ERRORA
						("|%s| CELLPHONE is not registered to network, consider to move it or additional antenna\n",
						 GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					tech_pvt->not_registered=1;
					tech_pvt->home_network_registered=0;
					tech_pvt->roaming_registered=0;
					alarm_event(tech_pvt, ALARM_NO_NETWORK_REGISTRATION, "CELLPHONE is not registered to network, consider to move it or additional antenna");
				} else if (stat==1) {
					DEBUGA_GSMOPEN("|%s| CELLPHONE is registered to the HOME network\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					tech_pvt->not_registered=0;
					tech_pvt->home_network_registered=1;
					tech_pvt->roaming_registered=0;
				}else {
					ERRORA("|%s| CELLPHONE is registered to a ROAMING network\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					tech_pvt->not_registered=0;
					tech_pvt->home_network_registered=0;
					tech_pvt->roaming_registered=1;
					alarm_event(tech_pvt, ALARM_ROAMING_NETWORK_REGISTRATION, "CELLPHONE is registered to a ROAMING network");
				}

			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CMGW:", 6) == 0)) {
				int err;

				err = sscanf(&tech_pvt->line_array.result[i][7], "%s", tech_pvt->at_cmgw);
				DEBUGA_GSMOPEN("|%s| +CMGW: %s\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], tech_pvt->at_cmgw);
				if (err < 1) {
					ERRORA("|%s| is not formatted as: |+CMGW: xxxx|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}

			}

			/* at_call_* are unsolicited messages sent by the modem to signal us about call processing activity and events */
			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_idle) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_IDLE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					DEBUGA_GSMOPEN("just received a remote HANGUP\n", GSMOPEN_P_LOG);
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NORMAL;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
					DEBUGA_GSMOPEN("just sent GSMOPEN_CONTROL_HANGUP\n", GSMOPEN_P_LOG);
				}

				tech_pvt->phone_callflow = CALLFLOW_CALL_NOCARRIER;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOCARRIER\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
					//cicopet
					switch_core_session_t *session = NULL;
					switch_channel_t *channel = NULL;

					tech_pvt->interface_state = GSMOPEN_STATE_DOWN;

					session = switch_core_session_locate(tech_pvt->session_uuid_str);
					if (session) {
						channel = switch_core_session_get_channel(session);
						//gsmopen_hangup(tech_pvt);
						switch_core_session_rwunlock(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NONE);
					}
					//
					//tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				} else {
					ERRORA("Why NO CARRIER now?\n", GSMOPEN_P_LOG);
				}











			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_incoming) == 0)) {

				//char list_command[64];

				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_INCOMING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				if (tech_pvt->phone_callflow != CALLFLOW_CALL_INCOMING && tech_pvt->phone_callflow != CALLFLOW_INCOMING_RING) {
					//mark the time of CALLFLOW_CALL_INCOMING
					gettimeofday(&(tech_pvt->call_incoming_time), NULL);
					tech_pvt->phone_callflow = CALLFLOW_CALL_INCOMING;
					DEBUGA_GSMOPEN("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld\n", GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec);

				}
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_active) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;
				DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_ACTIVE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				if (tech_pvt->interface_state == CALLFLOW_CALL_DIALING || tech_pvt->interface_state == CALLFLOW_STATUS_EARLYMEDIA) {
					DEBUGA_PBX("just received a remote ANSWER\n", GSMOPEN_P_LOG);
					if (tech_pvt->phone_callflow == GSMOPEN_STATE_UP) {
						//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_RINGING);
						DEBUGA_PBX("just sent GSMOPEN_CONTROL_RINGING\n", GSMOPEN_P_LOG);
						DEBUGA_PBX("going to send GSMOPEN_CONTROL_ANSWER\n", GSMOPEN_P_LOG);
						//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_ANSWER);
						tech_pvt->interface_state = CALLFLOW_CALL_REMOTEANSWER;
						DEBUGA_PBX("just sent GSMOPEN_CONTROL_ANSWER\n", GSMOPEN_P_LOG);
					}
				} else {
				}
				//tech_pvt->interface_state = GSMOPEN_STATE_UP;
				//DEBUGA_PBX("just interface_state UP\n", GSMOPEN_P_LOG);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_calling) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_DIALING;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_DIALING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}
			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_call_failed) == 0)) {
				tech_pvt->phone_callflow = CALLFLOW_CALL_FAILED;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_FAILED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CSCA:", 6) == 0)) {	//TODO SMS FIXME in config!
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CSCA: Message Center Address!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CMGF:", 6) == 0)) {	//TODO SMS FIXME in config!
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| +CMGF: Message Format!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CMTI:", 6) == 0)) {	//TODO SMS FIXME in config!
				int err;
				int pos;

				//FIXME all the following commands in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMTI: Incoming SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				err = sscanf(&tech_pvt->line_array.result[i][12], "%d", &pos);
				if (err < 1) {
					ERRORA("|%s| is not formatted as: |+CMTI: \"MT\",xx|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				} else {
					DEBUGA_GSMOPEN("|%s| +CMTI: Incoming SMS in position: %d!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i], pos);
					tech_pvt->unread_sms_msg_id = pos;
					gsmopen_sleep(1000);

					if (tech_pvt->unread_sms_msg_id) {
						char at_command[256];

						if (tech_pvt->no_ucs2 == 0) {
							res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"UCS2\"");
							if (res) {
								ERRORA("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone, continuing\n", GSMOPEN_P_LOG);
								//memset(tech_pvt->sms_message, 0, sizeof(tech_pvt->sms_message));
							}
						}

						memset(at_command, 0, sizeof(at_command));
						sprintf(at_command, "AT+CMGR=%d", tech_pvt->unread_sms_msg_id);
						//memset(tech_pvt->sms_message, 0, sizeof(tech_pvt->sms_message));

						tech_pvt->reading_sms_msg = 1;
						res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
						tech_pvt->reading_sms_msg = 0;
						if (res) {
							ERRORA("AT+CMGR (read SMS) do not got OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
						}
						res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"GSM\"");
						if (res) {
							ERRORA("AT+CSCS=\"GSM\" (set TE messages to GSM) do not got OK from the phone\n", GSMOPEN_P_LOG);
						}
						memset(at_command, 0, sizeof(at_command));
						sprintf(at_command, "AT+CMGD=%d", tech_pvt->unread_sms_msg_id);	/* delete the message */
						tech_pvt->unread_sms_msg_id = 0;
						res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
						if (res) {
							ERRORA("AT+CMGD (Delete SMS) do not got OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
						}

						res = sms_incoming(tech_pvt);

#if 0
						if (strlen(tech_pvt->sms_message)) {
							//FIXME manager_event(EVENT_FLAG_SYSTEM, "GSMOPENincomingsms",
							//FIXME "Interface: %s\r\nSMS_Message: %s\r\n", tech_pvt->name,
							//FIXME tech_pvt->sms_message);

							res = sms_incoming(tech_pvt, tech_pvt->sms_message);

							if (strlen(tech_pvt->sms_receiving_program)) {
								int fd1[2];
								pid_t pid1;
								char *arg1[] = { tech_pvt->sms_receiving_program, (char *) NULL };
								int i;

								DEBUGA_GSMOPEN("incoming SMS message:>>>%s<<<\n", GSMOPEN_P_LOG, tech_pvt->sms_message);
								res = pipe(fd1);
								pid1 = fork();

								if (pid1 == 0) {	//child
									int err;

									dup2(fd1[0], 0);	// Connect stdin to pipe output
									close(fd1[1]);	// close input pipe side
									close(tech_pvt->controldevfd);
									setsid();	//session id
									err = execvp(arg1[0], arg1);	//exec our program, with stdin connected to pipe output
									if (err) {
										ERRORA
											("'sms_receiving_program' is set in config file to '%s', and it gave us back this error: %d, (%s). SMS received was:---%s---\n",
											 GSMOPEN_P_LOG, tech_pvt->sms_receiving_program, err, strerror(errno), tech_pvt->sms_message);
									}
									close(fd1[0]);	// close output pipe side
								}
//starting here continue the parent
								close(fd1[0]);	// close output pipe side
								// write the msg on the pipe input
								for (i = 0; i < strlen(tech_pvt->sms_message); i++) {
									res = write(fd1[1], &tech_pvt->sms_message[i], 1);
								}
								close(fd1[1]);	// close pipe input, let our program know we've finished
							} else {
								ERRORA
									("got SMS incoming message, but 'sms_receiving_program' is not set in config file. SMS received was:---%s---\n",
									 GSMOPEN_P_LOG, tech_pvt->sms_message);
							}
						}
#endif //0
#if 1							//is this one needed? maybe it can interrupt an incoming call that is just to announce itself
						if (tech_pvt->phone_callflow == CALLFLOW_CALL_IDLE && tech_pvt->interface_state == GSMOPEN_STATE_DOWN && tech_pvt->owner == NULL) {
							/* we're not in a call, neither calling */
							res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CKPD=\"EEE\"");
							if (res) {
								ERRORA("AT+CKPD=\"EEE\" (cellphone screen back to user) do not got OK from the phone\n", GSMOPEN_P_LOG);
							}
						}
#endif
					}			//unread_msg_id

				}				//CMTI well formatted

			}					//CMTI

			if ((strncmp(tech_pvt->line_array.result[i], "+MMGL:", 6) == 0)) {	//TODO MOTOROLA SMS FIXME in config!
				int err = 0;
				//int unread_msg_id=0;

				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +MMGL: Listing Motorola SMSs!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

				err = sscanf(&tech_pvt->line_array.result[i][7], "%d", &tech_pvt->unread_sms_msg_id);
				if (err < 1) {
					ERRORA("|%s| is not formatted as: |+MMGL: xx|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}
			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CMGL:", 6) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGL: Listing SMSs!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}
			if ((strncmp(tech_pvt->line_array.result[i], "+MMGR:", 6) == 0)) {	//TODO MOTOROLA SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +MMGR: Reading Motorola SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->reading_sms_msg)
					tech_pvt->reading_sms_msg++;
			}
			if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"STO U", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading stored UNSENT SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"STO S", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading stored SENT SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"REC R", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading received READ SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: \"REC U", 13) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading received UNREAD SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->reading_sms_msg)
					tech_pvt->reading_sms_msg++;
			} else if ((strncmp(tech_pvt->line_array.result[i], "+CMGR: ", 6) == 0)) {	//TODO  SMS FIXME in config!
				if (option_debug)
					DEBUGA_GSMOPEN("|%s| +CMGR: Reading SMS!\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->reading_sms_msg)
					tech_pvt->reading_sms_msg++;
			}


			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 17") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_INFLUX;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_INFLUX\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 68") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_NOSERVICE;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_NOSERVICE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 70") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_OUTGOINGRESTRICTED;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_OUTGOINGRESTRICTED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "+MCST: 72") == 0)) {	/* motorola call processing unsolicited messages */
				tech_pvt->phone_callflow = CALLFLOW_CALL_SECURITYFAIL;
				if (option_debug > 1)
					DEBUGA_GSMOPEN("|%s| CALLFLOW_CALL_SECURITYFAIL\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN && tech_pvt->owner) {
					tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
					gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				}
			}

			if ((strncmp(tech_pvt->line_array.result[i], "+CPBR", 5) == 0)) {	/* phonebook stuff begins */

				if (tech_pvt->phonebook_querying) {	/* probably phonebook struct begins */
					int err, first_entry, last_entry, number_lenght, text_lenght;

					if (option_debug)
						DEBUGA_GSMOPEN("phonebook struct: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

					err = sscanf(&tech_pvt->line_array.result[i][8], "%d-%d),%d,%d", &first_entry, &last_entry, &number_lenght, &text_lenght);
					if (err < 4) {

						err = sscanf(&tech_pvt->line_array.result[i][7], "%d-%d,%d,%d", &first_entry, &last_entry, &number_lenght, &text_lenght);
					}

					if (err < 4) {
						ERRORA
							("phonebook struct: |%s| is nor formatted as: |+CPBR: (1-750),40,14| neither as: |+CPBR: 1-750,40,14|\n",
							 GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					} else {

						if (option_debug)
							DEBUGA_GSMOPEN
								("First entry: %d, last entry: %d, phone number max lenght: %d, text max lenght: %d\n",
								 GSMOPEN_P_LOG, first_entry, last_entry, number_lenght, text_lenght);
						tech_pvt->phonebook_first_entry = first_entry;
						tech_pvt->phonebook_last_entry = last_entry;
						tech_pvt->phonebook_number_lenght = number_lenght;
						tech_pvt->phonebook_text_lenght = text_lenght;
					}

				} else {		/* probably phonebook entry begins */

					if (tech_pvt->phonebook_listing) {
						int err, entry_id, entry_type;

						char entry_number[256];
						char entry_text[256];

						if (option_debug)
							DEBUGA_GSMOPEN("phonebook entry: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

						err =
							sscanf(&tech_pvt->line_array.result[i][7], "%d,\"%255[0-9+]\",%d,\"%255[^\"]\"", &entry_id, entry_number, &entry_type,
								   entry_text);
						if (err < 4) {
							ERRORA
								("err=%d, phonebook entry: |%s| is not formatted as: |+CPBR: 504,\"+39025458068\",145,\"ciao a tutti\"|\n",
								 GSMOPEN_P_LOG, err, tech_pvt->line_array.result[i]);
						} else {
							//TODO: sanitize entry_text
							if (option_debug)
								DEBUGA_GSMOPEN("Number: %s, Text: %s, Type: %d\n", GSMOPEN_P_LOG, entry_number, entry_text, entry_type);
							/* write entry in phonebook file */
							if (tech_pvt->phonebook_writing_fp) {
								gsmopen_dir_entry_extension++;

								fprintf(tech_pvt->phonebook_writing_fp,
										"%s  => ,%sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcell=%s|phonebook_entry_owner=%s\n",
										entry_number, entry_text, "no",
										tech_pvt->gsmopen_dir_entry_extension_prefix, "2", gsmopen_dir_entry_extension, "yes", "not_specified");
								fprintf(tech_pvt->phonebook_writing_fp,
										"%s  => ,%sDO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromcell=%s|phonebook_entry_owner=%s\n",
										entry_number, entry_text, "no",
										tech_pvt->gsmopen_dir_entry_extension_prefix, "3", gsmopen_dir_entry_extension, "yes", "not_specified");
							}
						}

					}

					if (tech_pvt->phonebook_listing_received_calls) {
						int err, entry_id, entry_type;

						char entry_number[256] = "";
						char entry_text[256] = "";

						if (option_debug)
							DEBUGA_GSMOPEN("phonebook entry: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

						err =
							sscanf(&tech_pvt->line_array.result[i][7], "%d,\"%255[0-9+]\",%d,\"%255[^\"]\"", &entry_id, entry_number, &entry_type,
								   entry_text);
						if (err < 1) {	//we match only on the progressive id, maybe the remote party has not sent its number, and/or there is no corresponding text entry in the phone directory
							ERRORA
								("err=%d, phonebook entry: |%s| is not formatted as: |+CPBR: 504,\"+39025458068\",145,\"ciao a tutti\"|\n",
								 GSMOPEN_P_LOG, err, tech_pvt->line_array.result[i]);
						} else {
							//TODO: sanitize entry_text

							if (option_debug)
								DEBUGA_GSMOPEN("Number: %s, Text: %s, Type: %d\n", GSMOPEN_P_LOG, entry_number, entry_text, entry_type);
							memset(tech_pvt->callid_name, 0, sizeof(tech_pvt->callid_name));
							memset(tech_pvt->callid_number, 0, sizeof(tech_pvt->callid_number));
							strncpy(tech_pvt->callid_name, entry_text, sizeof(tech_pvt->callid_name));
							strncpy(tech_pvt->callid_number, entry_number, sizeof(tech_pvt->callid_number));
							if (option_debug)
								DEBUGA_GSMOPEN("incoming callid: Text: %s, Number: %s\n", GSMOPEN_P_LOG, tech_pvt->callid_name, tech_pvt->callid_number);

							DEBUGA_GSMOPEN("|%s| CPBR INCOMING CALLID: name is %s, number is %s\n",
										   GSMOPEN_P_LOG, tech_pvt->line_array.result[i],
										   tech_pvt->callid_name[0] != 1 ? tech_pvt->callid_name : "not available",
										   tech_pvt->callid_number[0] ? tech_pvt->callid_number : "not available");

							/* mark the time of RING */
							gettimeofday(&(tech_pvt->ringtime), NULL);
							tech_pvt->interface_state = GSMOPEN_STATE_RING;
							tech_pvt->phone_callflow = CALLFLOW_INCOMING_RING;

						}

					}

					else {
						DEBUGA_GSMOPEN("phonebook entry: |%s|\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);

					}
				}

			}

			if ((strncmp(tech_pvt->line_array.result[i], "*ECAV", 5) == 0) || (strncmp(tech_pvt->line_array.result[i], "*ECAM", 5) == 0)) {	/* sony-ericsson call processing unsolicited messages */
				int res, ccid, ccstatus, calltype, processid, exitcause, number, type;
				res = ccid = ccstatus = calltype = processid = exitcause = number = type = 0;
				res =
					sscanf(&tech_pvt->line_array.result[i][6], "%d,%d,%d,%d,%d,%d,%d", &ccid, &ccstatus, &calltype, &processid, &exitcause, &number,
						   &type);
				/* only changes the phone_callflow if enought parameters were parsed */
				if (res >= 3) {
					switch (ccstatus) {
					case 0:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_DOWN);
							tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_NORMAL;
							gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
						}
						tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
						tech_pvt->interface_state = GSMOPEN_STATE_DOWN;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: IDLE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 1:
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: CALLING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 2:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_DIALING);
						}
						tech_pvt->interface_state = CALLFLOW_CALL_DIALING;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: CONNECTING\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 3:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_UP);
							gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_ANSWER);
						}
						tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;
						tech_pvt->interface_state = GSMOPEN_STATE_UP;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: ACTIVE\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 4:
						if (option_debug > 1)
							DEBUGA_GSMOPEN
								("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle HOLD event\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					case 5:
						if (option_debug > 1)
							DEBUGA_GSMOPEN
								("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle WAITING event\n", GSMOPEN_P_LOG,
								 tech_pvt->line_array.result[i]);
						break;
					case 6:
						if (option_debug > 1)
							DEBUGA_GSMOPEN
								("|%s| Sony-Ericsson *ECAM/*ECAV: don't know how to handle ALERTING event\n", GSMOPEN_P_LOG,
								 tech_pvt->line_array.result[i]);
						break;
					case 7:
						if (tech_pvt->owner) {
							ast_setstate(tech_pvt->owner, GSMOPEN_STATE_BUSY);
							gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_BUSY);
						}
						tech_pvt->phone_callflow = CALLFLOW_CALL_LINEBUSY;
						tech_pvt->interface_state = GSMOPEN_STATE_BUSY;
						if (option_debug > 1)
							DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: BUSY\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						break;
					}
				} else {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| Sony-Ericsson *ECAM/*ECAV: could not parse parameters\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
				}

			}

			/* at_indicator_* are unsolicited messages sent by the phone to signal us that some of its visual indicators on its screen has changed, based on CIND CMER ETSI docs */
			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_noservice_string) == 0)) {
					ERRORA("|%s| at_indicator_noservice_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						alarm_event(tech_pvt, ALARM_NETWORK_NO_SERVICE, "at_indicator_noservice_string");
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nosignal_string) == 0)) {
					ERRORA("|%s| at_indicator_nosignal_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						alarm_event(tech_pvt, ALARM_NETWORK_NO_SIGNAL, "at_indicator_nosignal_string");
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_lowsignal_string) == 0)) {
					WARNINGA("|%s| at_indicator_lowsignal_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
						alarm_event(tech_pvt, ALARM_NETWORK_LOW_SIGNAL, "at_indicator_lowsignal_string");
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_lowbattchg_string) == 0)) {
					WARNINGA("|%s| at_indicator_lowbattchg_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nobattchg_string) == 0)) {
					ERRORA("|%s| at_indicator_nobattchg_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callactive_string) == 0)) {
					DEBUGA_GSMOPEN("|%s| at_indicator_callactive_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nocallactive_string) == 0)) {
					DEBUGA_GSMOPEN("|%s| at_indicator_nocallactive_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_nocallsetup_string) == 0)) {
					DEBUGA_GSMOPEN("|%s| at_indicator_nocallsetup_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callsetupincoming_string) == 0)) {
					DEBUGA_GSMOPEN("|%s| at_indicator_callsetupincoming_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callsetupoutgoing_string) == 0)) {
					DEBUGA_GSMOPEN("|%s| at_indicator_callsetupoutgoing_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

			if ((strcmp(tech_pvt->line_array.result[i], tech_pvt->at_indicator_callsetupremoteringing_string)
				 == 0)) {
					DEBUGA_GSMOPEN("|%s| at_indicator_callsetupremoteringing_string\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
			}

		}

		/* let's look for OK, ERROR and EXPECTED_STRING in the complete lines read so far, without re-looking at the lines that has been yet looked at */
		for (i = la_read; i < la_counter; i++) {
			if (expected_string) {
				if ((strncmp(tech_pvt->line_array.result[i], expected_string, strlen(expected_string))
					 == 0)) {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("|%s| got what EXPECTED\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					at_ack = AT_OK;
				}
			} else {
				if ((strcmp(tech_pvt->line_array.result[i], "OK") == 0)) {
					if (option_debug > 1)
						DEBUGA_GSMOPEN("got OK\n", GSMOPEN_P_LOG);
					at_ack = AT_OK;
				}
			}
			if ((strcmp(tech_pvt->line_array.result[i], "ERROR") == 0)) {
				if (option_debug > 1)
					DEBUGA_GSMOPEN("got ERROR\n", GSMOPEN_P_LOG);
				at_ack = AT_ERROR;
			}

			/* if we are requesting IMEI, put the line into the imei buffer if the line is not "OK" or "ERROR" */
			if (tech_pvt->requesting_imei && at_ack == -1) {
				if (strlen(tech_pvt->line_array.result[i])) {	/* we are reading the IMEI */
					strncpy(tech_pvt->imei, tech_pvt->line_array.result[i], sizeof(tech_pvt->imei));
				}
			}

			/* if we are requesting IMSI, put the line into the imei buffer if the line is not "OK" or "ERROR" */
			if (tech_pvt->requesting_imsi && at_ack == -1) {
				if (strlen(tech_pvt->line_array.result[i])) {	/* we are reading the IMSI */
					strncpy(tech_pvt->imsi, tech_pvt->line_array.result[i], sizeof(tech_pvt->imsi));
				}
			}
			/* if we are reading an sms message from memory, put the line into the sms buffer if the line is not "OK" or "ERROR" */
			if (tech_pvt->reading_sms_msg > 1 && at_ack == -1) {
				int c;
				char sms_body[16000];
				//int err = 0;
				memset(sms_body, '\0', sizeof(sms_body));

				if (strncmp(tech_pvt->line_array.result[i], "+CMGR", 5) == 0) {	/* we are reading the "header" of an SMS */
#if 1
					char content[512];
					char content2[512];
					int inside_comma = 0;
					int inside_quote = 0;
					int which_field = 0;
					int d = 0;

					DEBUGA_GSMOPEN("HERE\n", GSMOPEN_P_LOG);

					memset(content, '\0', sizeof(content));


					for (c = 0; c < strlen(tech_pvt->line_array.result[i]); c++) {
						if (tech_pvt->line_array.result[i][c] == ',' && tech_pvt->line_array.result[i][c - 1] != '\\' && inside_quote == 0) {
							if (inside_comma) {
								inside_comma = 0;
								DEBUGA_GSMOPEN("inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
									   &tech_pvt->line_array.result[i][c]);
							} else {
								inside_comma = 1;
								DEBUGA_GSMOPEN("inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
									   &tech_pvt->line_array.result[i][c]);
							}
						}
						if (tech_pvt->line_array.result[i][c] == '"' && tech_pvt->line_array.result[i][c - 1] != '\\') {
							if (inside_quote) {
								inside_quote = 0;
								DEBUGA_GSMOPEN("END_CONTENT inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
									   &tech_pvt->line_array.result[i][c]);
								DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content);

								//strncat(tech_pvt->sms_message, "---", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
								//strncat(tech_pvt->sms_message, content, ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
								//strncat(tech_pvt->sms_message, "|||", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));

								memset(content2, '\0', sizeof(content2));
								if (which_field == 1) {
									//FIXME why this? err = ucs2_to_utf8(tech_pvt, content, content2, sizeof(content2));
									//err = ucs2_to_utf8(tech_pvt, content, content2, sizeof(content2));
									//err = 0;
									strncpy(content2, content, sizeof(content2));
								} else {
									//err = 0;
									strncpy(content2, content, sizeof(content2));
								}
								DEBUGA_GSMOPEN("%d content2=%s\n", GSMOPEN_P_LOG, which_field, content2);
								DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content2);

								//strncat(tech_pvt->sms_message, "---", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
								//if (!err)
								//strncat(tech_pvt->sms_message, content2, ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
								//strncat(tech_pvt->sms_message, "|||", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
								memset(content, '\0', sizeof(content));
								d = 0;
								if (which_field == 1) {
									strncpy(tech_pvt->sms_sender, content2, sizeof(tech_pvt->sms_sender));
									DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content2);

								} else if (which_field == 2) {
									strncpy(tech_pvt->sms_date, content2, sizeof(tech_pvt->sms_date));
									DEBUGA_GSMOPEN("%d content=%s\n", GSMOPEN_P_LOG, which_field, content2);
								} else if (which_field > 2) {
									WARNINGA("WHY which_field is > 2 ? (which_field is %d)\n", GSMOPEN_P_LOG, which_field);
								}
								which_field++;
							} else {
								inside_quote = 1;
								DEBUGA_GSMOPEN("START_CONTENT inside_comma=%d, inside_quote=%d, we're at=%s\n", GSMOPEN_P_LOG, inside_comma, inside_quote,
										 &tech_pvt->line_array.result[i][c]);
							}
						}
						if (inside_quote && tech_pvt->line_array.result[i][c] != '"') {

							content[d] = tech_pvt->line_array.result[i][c];
							d++;

						}

					}
#endif //0
				}				//it was the +CMGR answer from the cellphone
				else {
					DEBUGA_GSMOPEN("body=%s\n", GSMOPEN_P_LOG, sms_body);
					DEBUGA_GSMOPEN("tech_pvt->line_array.result[i]=%s\n", GSMOPEN_P_LOG, tech_pvt->line_array.result[i]);
					if (tech_pvt->sms_pdu_not_supported) {
						char content3[1000];
						strncpy(tech_pvt->sms_message, tech_pvt->line_array.result[i], sizeof(tech_pvt->sms_message));

						//int howmanyleft;


						DEBUGA_GSMOPEN("sms_message=%s\n", GSMOPEN_P_LOG, tech_pvt->sms_message);
						ucs2_to_utf8(tech_pvt, tech_pvt->sms_message, content3, sizeof(content3));
						DEBUGA_GSMOPEN("content3=%s\n", GSMOPEN_P_LOG, content3);
						strncpy(tech_pvt->sms_body, content3, sizeof(tech_pvt->sms_body));
						//sleep(10);
						//cicopet
						if (tech_pvt->sms_cnmi_not_supported) {
							sms_incoming(tech_pvt);
							DEBUGA_GSMOPEN("2 content3=%s\n", GSMOPEN_P_LOG, content3);
						}
					} else {
#ifndef NO_GSMLIB
						char content2[1000];
						SMSMessageRef sms;
//MessageType messagetype;
//Address servicecentreaddress;
//Timestamp servicecentretimestamp;
//Address sender_recipient_address;

						sms = SMSMessage::decode(tech_pvt->line_array.result[i]);	// dataCodingScheme = 8 , text=ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大

						DEBUGA_GSMOPEN("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());

						memset(content2, '\0', sizeof(content2));
						if (sms->dataCodingScheme().getAlphabet() == DCS_DEFAULT_ALPHABET) {
							iso_8859_1_to_utf8(tech_pvt, (char *) sms->userData().c_str(), content2, sizeof(content2));
						} else if (sms->dataCodingScheme().getAlphabet() == DCS_SIXTEEN_BIT_ALPHABET) {
							ucs2_to_utf8(tech_pvt, (char *) bufToHex((unsigned char *) sms->userData().data(), sms->userData().length()).c_str(), content2,
										 sizeof(content2));
						} else {
							ERRORA("dataCodingScheme not supported=%d\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());

						}
						DEBUGA_GSMOPEN("dataCodingScheme=%d\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());
						DEBUGA_GSMOPEN("dataCodingScheme=%s\n", GSMOPEN_P_LOG, sms->dataCodingScheme().toString().c_str());
						DEBUGA_GSMOPEN("address=%s\n", GSMOPEN_P_LOG, sms->address().toString().c_str());
						DEBUGA_GSMOPEN("serviceCentreAddress=%s\n", GSMOPEN_P_LOG, sms->serviceCentreAddress().toString().c_str());
						DEBUGA_GSMOPEN("serviceCentreTimestamp=%s\n", GSMOPEN_P_LOG, sms->serviceCentreTimestamp().toString().c_str());
						DEBUGA_GSMOPEN("UserDataHeader=%s\n", GSMOPEN_P_LOG, (char *)bufToHex((unsigned char *)
						                                ((string) sms->userDataHeader()).data(), sms->userDataHeader().length()).c_str());
						DEBUGA_GSMOPEN("messageType=%d\n", GSMOPEN_P_LOG, sms->messageType());
						DEBUGA_GSMOPEN("userData= |||%s|||\n", GSMOPEN_P_LOG, content2);


						memset(sms_body, '\0', sizeof(sms_body));
						strncpy(sms_body, content2, sizeof(sms_body));
						DEBUGA_GSMOPEN("body=%s\n", GSMOPEN_P_LOG, sms_body);
						strncpy(tech_pvt->sms_body, sms_body, sizeof(tech_pvt->sms_body));
						strncpy(tech_pvt->sms_sender, sms->address().toString().c_str(), sizeof(tech_pvt->sms_sender));
						strncpy(tech_pvt->sms_date, sms->serviceCentreTimestamp().toString().c_str(), sizeof(tech_pvt->sms_date));
						strncpy(tech_pvt->sms_userdataheader, (char *)
						                        bufToHex((unsigned char *)((string) sms->userDataHeader()).data(), sms->userDataHeader().length()).c_str(),
						                        sizeof(tech_pvt->sms_userdataheader));
						strncpy(tech_pvt->sms_datacodingscheme, sms->dataCodingScheme().toString().c_str(), sizeof(tech_pvt->sms_datacodingscheme));
						strncpy(tech_pvt->sms_servicecentreaddress, sms->serviceCentreAddress().toString().c_str(),
								sizeof(tech_pvt->sms_servicecentreaddress));
						tech_pvt->sms_messagetype = sms->messageType();
//messagetype = sms->messageType();
//servicecentreaddress = sms->serviceCentreAddress();
//servicecentretimestamp = sms->serviceCentreTimestamp();
//sender_recipient_address = sms->address();

#endif// NO_GSMLIB
					}

#if 0
					//strncat(tech_pvt->sms_message, "---", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
					//strncat(tech_pvt->sms_message, tech_pvt->line_array.result[i], ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
					//strncat(tech_pvt->sms_message, "|||", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));

					memset(sms_body, '\0', sizeof(sms_body));
					err = ucs2_to_utf8(tech_pvt, tech_pvt->line_array.result[i], sms_body, sizeof(sms_body));
					DEBUGA_GSMOPEN("body=%s\n", GSMOPEN_P_LOG, sms_body);
					strncpy(tech_pvt->sms_body, sms_body, sizeof(tech_pvt->sms_body));

					//strncat(tech_pvt->sms_message, "---", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
					//if (!err)
					//strncat(tech_pvt->sms_message, sms_body, ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));
					//strncat(tech_pvt->sms_message, "|||", ((sizeof(tech_pvt->sms_message) - strlen(tech_pvt->sms_message)) - 1));

					//DEBUGA_GSMOPEN("sms_message=%s\n", GSMOPEN_P_LOG, tech_pvt->sms_message);
#endif //0
				}				//it was the UCS2 from cellphone

			}					//we were reading the SMS

		}

		la_read = la_counter;

		if (look_for_ack && at_ack > -1)
			break;

		if (la_counter > AT_MESG_MAX_LINES) {
			ERRORA("Too many lines in result (>%d). Stopping reader.\n", GSMOPEN_P_LOG, AT_MESG_MAX_LINES);
			at_ack = AT_ERROR;
			break;
		}
	}

	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);
	if (select_err == -1) {
		ERRORA("select returned -1 on %s, setting controldev_dead, error was: %s\n", GSMOPEN_P_LOG, tech_pvt->controldevice_name, strerror(errno));
		tech_pvt->controldev_dead = 1;
		close(tech_pvt->controldevfd);

				tech_pvt->running=0;
				alarm_event(tech_pvt, ALARM_FAILED_INTERFACE, "select returned -1 on interface, setting controldev_dead");
				tech_pvt->active=0;
				tech_pvt->name[0]='\0';
		if (tech_pvt->owner)
			gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				switch_sleep(1000000);
		return -1;
	}

	if (tech_pvt->phone_callflow == CALLFLOW_CALL_INCOMING && tech_pvt->call_incoming_time.tv_sec) {	//after three sec of CALLFLOW_CALL_INCOMING, we assume the phone is incapable of notifying RING (eg: motorola c350), so we try to answer
		char list_command[64];
		struct timeval call_incoming_timeout;
		gettimeofday(&call_incoming_timeout, NULL);
		call_incoming_timeout.tv_sec -= 3;
		DEBUGA_GSMOPEN
			("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
			 GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
		if (call_incoming_timeout.tv_sec > tech_pvt->call_incoming_time.tv_sec) {

			tech_pvt->call_incoming_time.tv_sec = 0;
			tech_pvt->call_incoming_time.tv_usec = 0;
			DEBUGA_GSMOPEN
				("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
				 GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
			res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CPBS=RC");
			if (res) {
				ERRORA("AT+CPBS=RC (select memory of received calls) was not answered by the phone\n", GSMOPEN_P_LOG);
			}
			tech_pvt->phonebook_querying = 1;
			res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CPBR=?");
			if (res) {
				ERRORA("AT+CPBS=RC (select memory of received calls) was not answered by the phone\n", GSMOPEN_P_LOG);
			}
			tech_pvt->phonebook_querying = 0;
			sprintf(list_command, "AT+CPBR=%d,%d", tech_pvt->phonebook_first_entry, tech_pvt->phonebook_last_entry);
			tech_pvt->phonebook_listing_received_calls = 1;
			res = gsmopen_serial_write_AT_expect_longtime(tech_pvt, list_command, "OK");
			if (res) {
				WARNINGA("AT+CPBR=%d,%d failed, continue\n", GSMOPEN_P_LOG, tech_pvt->phonebook_first_entry, tech_pvt->phonebook_last_entry);
			}
			tech_pvt->phonebook_listing_received_calls = 0;
		}
	}

	if (tech_pvt->phone_callflow == CALLFLOW_INCOMING_RING) {
		struct timeval call_incoming_timeout;
		gettimeofday(&call_incoming_timeout, NULL);
		call_incoming_timeout.tv_sec -= 10;
		// DEBUGA_GSMOPEN ("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n", GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
		if (call_incoming_timeout.tv_sec > tech_pvt->ringtime.tv_sec) {
			ERRORA("Ringing stopped and I have not answered. Why?\n", GSMOPEN_P_LOG);
			DEBUGA_GSMOPEN
				("CALLFLOW_CALL_INCOMING call_incoming_time.tv_sec=%ld, call_incoming_timeout.tv_sec=%ld\n",
				 GSMOPEN_P_LOG, tech_pvt->call_incoming_time.tv_sec, call_incoming_timeout.tv_sec);
			if (tech_pvt->owner) {
				gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_HANGUP);
				tech_pvt->owner->hangupcause = GSMOPEN_CAUSE_FAILURE;
			}
		}
	}
	tech_pvt->line_array.elemcount = la_counter;
	//NOTICA (" OUTSIDE this gsmopen_serial_device %s \n", GSMOPEN_P_LOG, tech_pvt->controldevice_name);
	if (look_for_ack)
		return at_ack;
	else
		return 0;
}

int gsmopen_serial_write_AT(private_t * tech_pvt, const char *data)
{
	int howmany;
	int i;
	int res;
	int count;

	howmany = strlen(data);

	for (i = 0; i < howmany; i++) {
		res = write(tech_pvt->controldevfd, &data[i], 1);

		if (res != 1) {
			DEBUGA_GSMOPEN("Error sending (%.1s): %d (%s)\n", GSMOPEN_P_LOG, &data[i], res, strerror(errno));
			gsmopen_sleep(100000);
			for (count = 0; count < 10; count++) {
				res = write(tech_pvt->controldevfd, &data[i], 1);
				if (res == 1) {
					DEBUGA_GSMOPEN("Successfully RE-sent (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &data[i], count, res, strerror(errno));
					break;
				} else
					DEBUGA_GSMOPEN("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &data[i], count, res, strerror(errno));
				gsmopen_sleep(100000);

			}
			if (res != 1) {
				ERRORA("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &data[i], count, res, strerror(errno));
				return -1;
			}
		}
		if (option_debug > 1)
			DEBUGA_GSMOPEN("sent data... (%.1s)\n", GSMOPEN_P_LOG, &data[i]);
		gsmopen_sleep(1000);			/* release the cpu */
	}

	res = write(tech_pvt->controldevfd, "\r", 1);

	if (res != 1) {
		DEBUGA_GSMOPEN("Error sending (carriage return): %d (%s)\n", GSMOPEN_P_LOG, res, strerror(errno));
		gsmopen_sleep(100000);
		for (count = 0; count < 10; count++) {
			res = write(tech_pvt->controldevfd, "\r", 1);

			if (res == 1) {
				DEBUGA_GSMOPEN("Successfully RE-sent carriage return: %d %d (%s)\n", GSMOPEN_P_LOG, count, res, strerror(errno));
				break;
			} else
				DEBUGA_GSMOPEN("Error RE-sending (carriage return): %d %d (%s)\n", GSMOPEN_P_LOG, count, res, strerror(errno));
			gsmopen_sleep(100000);

		}
		if (res != 1) {
			ERRORA("Error RE-sending (carriage return): %d %d (%s)\n", GSMOPEN_P_LOG, count, res, strerror(errno));
			return -1;
		}
	}
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sent (carriage return)\n", GSMOPEN_P_LOG);
	gsmopen_sleep(1000);				/* release the cpu */

	return howmany;
}

int gsmopen_serial_write_AT_nocr(private_t * tech_pvt, const char *data)
{
	int howmany;
	int i;
	int res;
	int count;

	howmany = strlen(data);

	for (i = 0; i < howmany; i++) {
		res = write(tech_pvt->controldevfd, &data[i], 1);

		if (res != 1) {
			DEBUGA_GSMOPEN("Error sending (%.1s): %d (%s)\n", GSMOPEN_P_LOG, &data[i], res, strerror(errno));
			gsmopen_sleep(100000);
			for (count = 0; count < 10; count++) {
				res = write(tech_pvt->controldevfd, &data[i], 1);
				if (res == 1)
					break;
				else
					DEBUGA_GSMOPEN("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &data[i], count, res, strerror(errno));
				gsmopen_sleep(100000);

			}
			if (res != 1) {
				ERRORA("Error RE-sending (%.1s): %d %d (%s)\n", GSMOPEN_P_LOG, &data[i], count, res, strerror(errno));
				return -1;
			}
		}
		if (option_debug > 1)
			DEBUGA_GSMOPEN("sent data... (%.1s)\n", GSMOPEN_P_LOG, &data[i]);
		gsmopen_sleep(1000);			/* release the cpu */
	}

	gsmopen_sleep(1000);				/* release the cpu */

	return howmany;
}

int gsmopen_serial_write_AT_noack(private_t * tech_pvt, const char *data)
{

	if (option_debug > 1)
		DEBUGA_GSMOPEN("gsmopen_serial_write_AT_noack: %s\n", GSMOPEN_P_LOG, data);

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (gsmopen_serial_write_AT(tech_pvt, data) != strlen(data)) {

		ERRORA("Error sending data... (%s)\n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);
		return -1;
	}
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return 0;
}

int gsmopen_serial_write_AT_ack(private_t * tech_pvt, const char *data)
{
	int at_result = AT_ERROR;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sending: %s\n", GSMOPEN_P_LOG, data);
	if (gsmopen_serial_write_AT(tech_pvt, data) != strlen(data)) {
		ERRORA("Error sending data... (%s) \n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);
		return -1;
	}

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 500000, 2, NULL, 1);	// 2.5 sec timeout
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_write_AT_ack_nocr_longtime(private_t * tech_pvt, const char *data)
{
	int at_result = AT_ERROR;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sending: %s\n", GSMOPEN_P_LOG, data);
	if (gsmopen_serial_write_AT_nocr(tech_pvt, data) != strlen(data)) {
		ERRORA("Error sending data... (%s) \n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);
		return -1;
	}

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 500000, 20, NULL, 1);	// 20.5 sec timeout
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_write_AT_expect1(private_t * tech_pvt, const char *data, const char *expected_string, int expect_crlf, int seconds)
{
	int at_result = AT_ERROR;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("sending: %s, expecting: %s\n", GSMOPEN_P_LOG, data, expected_string);
	if (gsmopen_serial_write_AT(tech_pvt, data) != strlen(data)) {
		ERRORA("Error sending data... (%s) \n", GSMOPEN_P_LOG, strerror(errno));
		UNLOCKA(tech_pvt->controldev_lock);
		return -1;
	}

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 500000, seconds, expected_string, expect_crlf);	// 20.5 sec timeout, used for querying the SIM and sending SMSs
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_AT_expect(private_t * tech_pvt, const char *expected_string, int expect_crlf, int seconds)
{
	int at_result = AT_ERROR;

	PUSHA_UNLOCKA(tech_pvt->controldev_lock);
	LOKKA(tech_pvt->controldev_lock);
	if (option_debug > 1)
		DEBUGA_GSMOPEN("expecting: %s\n", GSMOPEN_P_LOG, expected_string);

	at_result = gsmopen_serial_read_AT(tech_pvt, 1, 500000, seconds, expected_string, expect_crlf);	// 20.5 sec timeout, used for querying the SIM and sending SMSs
	UNLOCKA(tech_pvt->controldev_lock);
	POPPA_UNLOCKA(tech_pvt->controldev_lock);

	return at_result;

}

int gsmopen_serial_answer(private_t * tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_answer_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_answer_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_answer_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */
	return -1;
}


int gsmopen_serial_answer_AT(private_t * tech_pvt)
{
	int res;

	res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_answer, tech_pvt->at_answer_expect);
	if (res) {
		DEBUGA_GSMOPEN
			("at_answer command failed, command used: %s, expecting: %s, trying with AT+CKPD=\"S\"\n",
			 GSMOPEN_P_LOG, tech_pvt->at_answer, tech_pvt->at_answer_expect);

		res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CKPD=\"S\"");
		if (res) {
			ERRORA("at_answer command failed, command used: 'AT+CKPD=\"S\"', giving up\n", GSMOPEN_P_LOG);
			return -1;
		}
	}
	//tech_pvt->interface_state = GSMOPEN_STATE_UP;
	//tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;
	DEBUGA_GSMOPEN("AT: call answered\n", GSMOPEN_P_LOG);
	return 0;
}

int gsmopen_serial_hangup(private_t * tech_pvt)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_hangup_AT(tech_pvt);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_hangup_FBUS2(tech_pvt);
#endif /* GSMOPEN_FBUS2 */
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_hangup_CVM_BUSMAIL(tech_pvt);
#endif /* GSMOPEN_CVM */
	return -1;
}


int gsmopen_serial_hangup_AT(private_t * tech_pvt)
{
	int res;

	if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
		res = gsmopen_serial_write_AT_expect(tech_pvt, tech_pvt->at_hangup, tech_pvt->at_hangup_expect);
		if (res) {
			DEBUGA_GSMOPEN("at_hangup command failed, command used: %s, trying to use AT+CKPD=\"EEE\"\n", GSMOPEN_P_LOG, tech_pvt->at_hangup);
			res = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CKPD=\"EEE\"");
			if (res) {
				ERRORA("at_hangup command failed, command used: 'AT+CKPD=\"EEE\"'\n", GSMOPEN_P_LOG);
				return -1;
			}
		}
	}
	tech_pvt->interface_state = GSMOPEN_STATE_DOWN;
	tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
	return 0;
}


int gsmopen_serial_call(private_t * tech_pvt, char *dstr)
{
	if (tech_pvt->controldevprotocol == PROTOCOL_AT)
		return gsmopen_serial_call_AT(tech_pvt, dstr);
#ifdef GSMOPEN_FBUS2
	if (tech_pvt->controldevprotocol == PROTOCOL_FBUS2)
		return gsmopen_serial_call_FBUS2(tech_pvt, dstr);
#endif /* GSMOPEN_FBUS2 */
	if (tech_pvt->controldevprotocol == PROTOCOL_NO_SERIAL)
		return 0;
#ifdef GSMOPEN_CVM
	if (tech_pvt->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
		return gsmopen_serial_call_CVM_BUSMAIL(tech_pvt, dstr);
#endif /* GSMOPEN_CVM */
	return -1;
}

int gsmopen_serial_call_AT(private_t * tech_pvt, char *dstr)
{
	int res;
	char at_command[256];

	if (option_debug)
		DEBUGA_PBX("Dialing %s\n", GSMOPEN_P_LOG, dstr);
	memset(at_command, 0, sizeof(at_command));
	tech_pvt->phone_callflow = CALLFLOW_CALL_DIALING;
	tech_pvt->interface_state = GSMOPEN_STATE_DIALING;
	//ast_uri_decode(dstr);
/*
  size_t fixdstr = strspn(dstr, AST_DIGIT_ANYDIG);
  if (fixdstr == 0) {
    ERRORA("dial command failed because of invalid dial number. dial string was: %s\n",
           GSMOPEN_P_LOG, dstr);
    return -1;
  }
*/
	//dstr[fixdstr] = '\0';
	sprintf(at_command, "%s%s%s", tech_pvt->at_dial_pre_number, dstr, tech_pvt->at_dial_post_number);
	DEBUGA_PBX("Dialstring %s\n", GSMOPEN_P_LOG, at_command);
	res = gsmopen_serial_write_AT_expect(tech_pvt, at_command, tech_pvt->at_dial_expect);
	if (res) {
		ERRORA("dial command failed, dial string was: %s\n", GSMOPEN_P_LOG, at_command);
		return -1;
	}
	// jet - early audio
	//if (tech_pvt->at_early_audio) {
	//ast_queue_control(tech_pvt->owner, AST_CONTROL_ANSWER);
	//}

	return 0;
}

int ucs2_to_utf8(private_t * tech_pvt, char *ucs2_in, char *utf8_out, size_t outbytesleft)
{
	char converted[16000];
#ifndef WIN32
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;
	size_t inbytesleft;
	int c;
	char stringa[5];
	double hexnum;
	int i = 0;

	memset(converted, '\0', sizeof(converted));

	DEBUGA_GSMOPEN("ucs2_in=%s\n", GSMOPEN_P_LOG, ucs2_in);
	/* cicopet */
	for (c = 0; c < strlen(ucs2_in); c++) {
		sprintf(stringa, "0x%c%c", ucs2_in[c], ucs2_in[c + 1]);
		c++;
		hexnum = strtod(stringa, NULL);
		converted[i] = (char) hexnum;
		i++;
	}

	outbuf = utf8_out;
	inbuf = converted;

	iconv_format = iconv_open("UTF8", "UCS-2BE");
	if (iconv_format == (iconv_t) - 1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}

	inbytesleft = i;
	DEBUGA_GSMOPEN("1 ciao in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
				   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, converted, utf8_out);

	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if (iconv_res == (size_t) -1) {
		DEBUGA_GSMOPEN("2 ciao in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
					   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, converted, utf8_out);
		DEBUGA_GSMOPEN("3 error: %s %d\n", GSMOPEN_P_LOG, strerror(errno), errno);
		iconv_close(iconv_format);
		return -1;
	}
	DEBUGA_GSMOPEN
		("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, converted=%s, utf8_out=%s\n",
		 GSMOPEN_P_LOG, iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, converted, utf8_out);
	iconv_close(iconv_format);

#endif //WIN32
	return 0;
}

int iso_8859_1_to_utf8(private_t * tech_pvt, char *iso_8859_1_in, char *utf8_out, size_t outbytesleft)
{
	char converted[16000];
#ifndef WIN32
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;
	size_t inbytesleft;
	//int c;
	//char stringa[5];
	//double hexnum;
	//int i = 0;

	memset(converted, '\0', sizeof(converted));

	DEBUGA_GSMOPEN("iso_8859_1_in=%s\n", GSMOPEN_P_LOG, iso_8859_1_in);

	outbuf = utf8_out;
	inbuf = iso_8859_1_in;

	iconv_format = iconv_open("UTF8", "ISO_8859-1");
	if (iconv_format == (iconv_t) - 1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}


	inbytesleft = strlen(iso_8859_1_in) * 2;
	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if (iconv_res == (size_t) -1) {
		DEBUGA_GSMOPEN("ciao in=%s, inleft=%d, out=%s, outleft=%d, utf8_out=%s\n",
					   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_out);
		DEBUGA_GSMOPEN("error: %s %d\n", GSMOPEN_P_LOG, strerror(errno), errno);
		return -1;
	}
	DEBUGA_GSMOPEN
		(" strlen(iso_8859_1_in)=%d, iconv_res=%d,  inbuf=%s, inleft=%d, out=%s, outleft=%d, utf8_out=%s\n",
		 GSMOPEN_P_LOG, (int) strlen(iso_8859_1_in), iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf8_out);



	iconv_close(iconv_format);

#endif //WIN32
	return 0;
}


int utf_to_ucs2(private_t * tech_pvt, char *utf_in, size_t inbytesleft, char *ucs2_out, size_t outbytesleft)
{
	/* cicopet */
#ifndef WIN32
	iconv_t iconv_format;
	int iconv_res;
	char *outbuf;
	char *inbuf;
	char converted[16000];
	int i;
	char stringa[16];
	char stringa2[16];

	memset(converted, '\0', sizeof(converted));

	outbuf = converted;
	inbuf = utf_in;

	iconv_format = iconv_open("UCS-2BE", "UTF8");
	if (iconv_format == (iconv_t) - 1) {
		ERRORA("error: %s\n", GSMOPEN_P_LOG, strerror(errno));
		return -1;
	}
	outbytesleft = 16000;

	DEBUGA_GSMOPEN("in=%s, inleft=%d, out=%s, outleft=%d, utf_in=%s, converted=%s\n",
				   GSMOPEN_P_LOG, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf_in, converted);
	iconv_res = iconv(iconv_format, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	if (iconv_res == (size_t) -1) {
		ERRORA("error: %s %d\n", GSMOPEN_P_LOG, strerror(errno), errno);
		return -1;
	}
	DEBUGA_GSMOPEN
		("iconv_res=%d,  in=%s, inleft=%d, out=%s, outleft=%d, utf_in=%s, converted=%s\n",
		 GSMOPEN_P_LOG, iconv_res, inbuf, (int) inbytesleft, outbuf, (int) outbytesleft, utf_in, converted);
	iconv_close(iconv_format);

	for (i = 0; i < 16000 - outbytesleft; i++) {
		memset(stringa, '\0', sizeof(stringa));
		memset(stringa2, '\0', sizeof(stringa2));
		sprintf(stringa, "%02X", converted[i]);
		DEBUGA_GSMOPEN("character is |%02X|\n", GSMOPEN_P_LOG, converted[i]);
		stringa2[0] = stringa[strlen(stringa) - 2];
		stringa2[1] = stringa[strlen(stringa) - 1];
		strncat(ucs2_out, stringa2, ((outbytesleft - strlen(ucs2_out)) - 1));	//add the received line to the buffer
		DEBUGA_GSMOPEN("stringa=%s, stringa2=%s, ucs2_out=%s\n", GSMOPEN_P_LOG, stringa, stringa2, ucs2_out);
	}
#endif //WIN32
	return 0;
}


/*! \brief  Answer incoming call,
 * Part of PBX interface */
int gsmopen_answer(private_t * tech_pvt)
{
	int res;

	if (option_debug) {
		DEBUGA_PBX("ENTERING FUNC\n", GSMOPEN_P_LOG);
	}
	/* do something to actually answer the call, if needed (eg. pick up the phone) */
	if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL) {
		if (gsmopen_serial_answer(tech_pvt)) {
			ERRORA("gsmopen_answer FAILED\n", GSMOPEN_P_LOG);
			if (option_debug) {
				DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
			}
			return -1;
		}
	}
	tech_pvt->interface_state = GSMOPEN_STATE_UP;
	tech_pvt->phone_callflow = CALLFLOW_CALL_ACTIVE;

	while (tech_pvt->interface_state == GSMOPEN_STATE_RING) {
		gsmopen_sleep(10000);			//10msec
	}
	if (tech_pvt->interface_state != GSMOPEN_STATE_UP) {
		ERRORA("call answering failed\n", GSMOPEN_P_LOG);
		res = -1;
	} else {
		if (option_debug)
			DEBUGA_PBX("call answered\n", GSMOPEN_P_LOG);
		res = 0;
#ifdef GSMOPEN_PORTAUDIO
		//speex_echo_state_reset(tech_pvt->stream->echo_state);
#endif // GSMOPEN_PORTAUDIO

		new_inbound_channel(tech_pvt);
		if (tech_pvt->owner) {
			DEBUGA_PBX("going to send GSMOPEN_STATE_UP\n", GSMOPEN_P_LOG);
			ast_setstate(tech_pvt->owner, GSMOPEN_STATE_UP);
			//ast_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_ANSWER);
			//gsmopen_queue_control(tech_pvt->owner, GSMOPEN_CONTROL_ANSWER);
			DEBUGA_PBX("just sent GSMOPEN_STATE_UP\n", GSMOPEN_P_LOG);
		}
	}
	if (option_debug) {
		DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
	}
	return res;
}

int gsmopen_ring(private_t * tech_pvt)
{
	int res = 0;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (option_debug) {
		//DEBUGA_PBX("ENTERING FUNC\n", GSMOPEN_P_LOG);
	}

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	if (session) {
		switch_core_session_rwunlock(session);
		return 0;
	}

	new_inbound_channel(tech_pvt);

	gsmopen_sleep(10000);

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	if (session) {
		channel = switch_core_session_get_channel(session);

		switch_core_session_queue_indication(session, SWITCH_MESSAGE_INDICATE_RINGING);
		if (channel) {
			switch_channel_mark_ring_ready(channel);
		} else {
			ERRORA("no session\n", GSMOPEN_P_LOG);
		}
		switch_core_session_rwunlock(session);
	} else {
		ERRORA("no session\n", GSMOPEN_P_LOG);

	}


	if (option_debug) {
		DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
	}
	return res;
}


/*! \brief  Hangup gsmopen call
 * Part of PBX interface, called from ast_hangup */

int gsmopen_hangup(private_t * tech_pvt)
{

	/* if there is not gsmopen pvt why we are here ? */
	if (!tech_pvt) {
		ERRORA("Asked to hangup channel not connected\n", GSMOPEN_P_LOG);
		return 0;
	}

	DEBUGA_GSMOPEN("ENTERING FUNC\n", GSMOPEN_P_LOG);


	if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL) {
		if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
			/* actually hangup through the serial port */
			if (tech_pvt->controldevprotocol != PROTOCOL_NO_SERIAL) {
				int res;
				res = gsmopen_serial_hangup(tech_pvt);
				if (res) {
					ERRORA("gsmopen_serial_hangup error: %d\n", GSMOPEN_P_LOG, res);
					if (option_debug) {
						DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
					}
					return -1;
				}
			}

			while (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
				gsmopen_sleep(10000);	//10msec
			}
			if (tech_pvt->interface_state != GSMOPEN_STATE_DOWN) {
				ERRORA("call hangup failed\n", GSMOPEN_P_LOG);
				return -1;
			} else {
				DEBUGA_GSMOPEN("call hungup\n", GSMOPEN_P_LOG);
			}
		}
	} else {
		tech_pvt->interface_state = GSMOPEN_STATE_DOWN;
		tech_pvt->phone_callflow = CALLFLOW_CALL_IDLE;
	}

	switch_set_flag(tech_pvt, TFLAG_HANGUP);
	if (option_debug) {
		DEBUGA_PBX("EXITING FUNC\n", GSMOPEN_P_LOG);
	}
	return 0;
}



#ifdef GSMOPEN_ALSA
/*! \brief ALSA pcm format, according to endianess  */
#if __BYTE_ORDER == __LITTLE_ENDIAN
snd_pcm_format_t gsmopen_format = SND_PCM_FORMAT_S16_LE;
#else
snd_pcm_format_t gsmopen_format = SND_PCM_FORMAT_S16_BE;
#endif

/*!
 * \brief Initialize the ALSA soundcard channels (capture AND playback) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the gsmopen_pvt of the interface
 *
 * This function call alsa_open_dev to initialize the ALSA soundcard for each channel (capture AND playback) used by one interface (a multichannel soundcard can be used by multiple interfaces). Called by sound_init
 *
 * \return zero on success, -1 on error.
 */
int alsa_init(private_t * tech_pvt)
{
	tech_pvt->alsac = alsa_open_dev(tech_pvt, SND_PCM_STREAM_CAPTURE);
	if (!tech_pvt->alsac) {
		ERRORA("Failed opening ALSA capture device: %s\n", GSMOPEN_P_LOG, tech_pvt->alsacname);
		if (alsa_shutdown(tech_pvt)) {
			ERRORA("alsa_shutdown failed\n", GSMOPEN_P_LOG);
			return -1;
		}
		return -1;
	}
	tech_pvt->alsap = alsa_open_dev(tech_pvt, SND_PCM_STREAM_PLAYBACK);
	if (!tech_pvt->alsap) {
		ERRORA("Failed opening ALSA playback device: %s\n", GSMOPEN_P_LOG, tech_pvt->alsapname);
		if (alsa_shutdown(tech_pvt)) {
			ERRORA("alsa_shutdown failed\n", GSMOPEN_P_LOG);
			return -1;
		}
		return -1;
	}

	/* make valgrind very happy */
	snd_config_update_free_global();
	return 0;
}

/*!
 * \brief Shutdown the ALSA soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the gsmopen_pvt of the interface
 *
 * This function shutdown the ALSA soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces). Called by sound_init
 *
 * \return zero on success, -1 on error.
 */

int alsa_shutdown(private_t * tech_pvt)
{

	int err;

	if (tech_pvt->alsap) {
		err = snd_pcm_drop(tech_pvt->alsap);
		if (err < 0) {
			ERRORA("device [%s], snd_pcm_drop failed with error '%s'\n", GSMOPEN_P_LOG, tech_pvt->alsapname, snd_strerror(err));
			return -1;
		}
		err = snd_pcm_close(tech_pvt->alsap);
		if (err < 0) {
			ERRORA("device [%s], snd_pcm_close failed with error '%s'\n", GSMOPEN_P_LOG, tech_pvt->alsapname, snd_strerror(err));
			return -1;
		}
		tech_pvt->alsap = NULL;
	}
	if (tech_pvt->alsac) {
		err = snd_pcm_drop(tech_pvt->alsac);
		if (err < 0) {
			ERRORA("device [%s], snd_pcm_drop failed with error '%s'\n", GSMOPEN_P_LOG, tech_pvt->alsacname, snd_strerror(err));
			return -1;
		}
		err = snd_pcm_close(tech_pvt->alsac);
		if (err < 0) {
			ERRORA("device [%s], snd_pcm_close failed with error '%s'\n", GSMOPEN_P_LOG, tech_pvt->alsacname, snd_strerror(err));
			return -1;
		}
		tech_pvt->alsac = NULL;
	}

	return 0;
}

/*!
 * \brief Setup and open the ALSA device (capture OR playback) 
 * \param p the gsmopen_pvt of the interface
 * \param stream the ALSA capture/playback definition
 *
 * This function setup and open the ALSA device (capture OR playback). Called by alsa_init
 *
 * \return zero on success, -1 on error.
 */
snd_pcm_t *alsa_open_dev(private_t * tech_pvt, snd_pcm_stream_t stream)
{

	snd_pcm_t *handle = NULL;
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_uframes_t buffer_size;
	int err;
	size_t n;
	//snd_pcm_uframes_t xfer_align;
	unsigned int rate;
	snd_pcm_uframes_t start_threshold, stop_threshold;
	snd_pcm_uframes_t period_size = 0;
	snd_pcm_uframes_t chunk_size = 0;
	int start_delay = 0;
	int stop_delay = 0;
	snd_pcm_state_t state;
	snd_pcm_info_t *info;
	unsigned int chan_num;

	period_size = tech_pvt->alsa_period_size;

	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);

	if (stream == SND_PCM_STREAM_CAPTURE) {
		err = snd_pcm_open(&handle, tech_pvt->alsacname, stream, 0 | SND_PCM_NONBLOCK);
	} else {
		err = snd_pcm_open(&handle, tech_pvt->alsapname, stream, 0 | SND_PCM_NONBLOCK);
	}
	if (err < 0) {
		ERRORA
			("snd_pcm_open failed with error '%s' on device '%s', if you are using a plughw:n device please change it to be a default:n device (so to allow it to be shared with other concurrent programs), or maybe you are using an ALSA voicemodem and slmodemd"
			 " is running?\n", GSMOPEN_P_LOG, snd_strerror(err), stream == SND_PCM_STREAM_CAPTURE ? tech_pvt->alsacname : tech_pvt->alsapname);
		return NULL;
	}

	snd_pcm_info_alloca(&info);

	if ((err = snd_pcm_info(handle, info)) < 0) {
		ERRORA("info error: %s", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}

	err = snd_pcm_nonblock(handle, 1);
	if (err < 0) {
		ERRORA("nonblock setting error: %s", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}

	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		ERRORA("Broken configuration for this PCM, no configurations available: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}

	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		ERRORA("Access type not available: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}
	err = snd_pcm_hw_params_set_format(handle, params, gsmopen_format);
	if (err < 0) {
		ERRORA("Sample format non available: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}
	err = snd_pcm_hw_params_set_channels(handle, params, 1);
	if (err < 0) {
		DEBUGA_GSMOPEN("Channels count set failed: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
	}
#if 1
	err = snd_pcm_hw_params_get_channels(params, &chan_num);
	if (err < 0) {
		ERRORA("Channels count non available: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}
	if (chan_num < 1 || chan_num > 2) {
		ERRORA("Channels count MUST BE 1 or 2, it is: %d\n", GSMOPEN_P_LOG, chan_num);
		ERRORA("Channels count MUST BE 1 or 2, it is: %d on %s %s\n", GSMOPEN_P_LOG, chan_num, tech_pvt->alsapname, tech_pvt->alsacname);
		return NULL;
	} else {
		if (chan_num == 1) {
			if (stream == SND_PCM_STREAM_CAPTURE)
				tech_pvt->alsa_capture_is_mono = 1;
			else
				tech_pvt->alsa_play_is_mono = 1;
		} else {
			if (stream == SND_PCM_STREAM_CAPTURE)
				tech_pvt->alsa_capture_is_mono = 0;
			else
				tech_pvt->alsa_play_is_mono = 0;
		}
	}
#else
	tech_pvt->alsa_capture_is_mono = 1;
	tech_pvt->alsa_play_is_mono = 1;
#endif

#if 1
	rate = tech_pvt->gsmopen_sound_rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
	if ((float) tech_pvt->gsmopen_sound_rate * 1.05 < rate || (float) tech_pvt->gsmopen_sound_rate * 0.95 > rate) {
		WARNINGA("Rate is not accurate (requested = %iHz, got = %iHz)\n", GSMOPEN_P_LOG, tech_pvt->gsmopen_sound_rate, rate);
	}

	if (err < 0) {
		ERRORA("Error setting rate: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}
	tech_pvt->gsmopen_sound_rate = rate;

	err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0);

	if (err < 0) {
		ERRORA("Error setting period_size: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}

	tech_pvt->alsa_period_size = period_size;

	tech_pvt->alsa_buffer_size = tech_pvt->alsa_period_size * tech_pvt->alsa_periods_in_buffer;

	err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &tech_pvt->alsa_buffer_size);

	if (err < 0) {
		ERRORA("Error setting buffer_size: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}
#endif

	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		ERRORA("Unable to install hw params: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}

	snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	if (chunk_size == buffer_size) {
		ERRORA("Can't use period equal to buffer size (%lu == %lu)\n", GSMOPEN_P_LOG, chunk_size, buffer_size);
		return NULL;
	}

	snd_pcm_sw_params_current(handle, swparams);

	/*
	   if (sleep_min)
	   xfer_align = 1;
	   err = snd_pcm_sw_params_set_sleep_min(handle, swparams,
	   0);

	   if (err < 0) {
	   ERRORA("Error setting slep_min: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
	   }
	 */
	n = chunk_size;
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);
	if (err < 0) {
		ERRORA("Error setting avail_min: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
	}
	if (stream == SND_PCM_STREAM_CAPTURE) {
		start_delay = 1;
	}
	if (start_delay <= 0) {
		start_threshold = n + (snd_pcm_uframes_t) rate *start_delay / 1000000;
	} else {
		start_threshold = (snd_pcm_uframes_t) rate *start_delay / 1000000;
	}
	if (start_threshold < 1)
		start_threshold = 1;
	if (start_threshold > n)
		start_threshold = n;
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
	if (err < 0) {
		ERRORA("Error setting start_threshold: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
	}

	if (stop_delay <= 0)
		stop_threshold = buffer_size + (snd_pcm_uframes_t) rate *stop_delay / 1000000;
	else
		stop_threshold = (snd_pcm_uframes_t) rate *stop_delay / 1000000;

	if (stream == SND_PCM_STREAM_CAPTURE) {
		stop_threshold = -1;
	}

	err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);

	if (err < 0) {
		ERRORA("Error setting stop_threshold: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
	}

	if (snd_pcm_sw_params(handle, swparams) < 0) {
		ERRORA("Error installing software parameters: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
	}

	err = snd_pcm_poll_descriptors_count(handle);
	if (err <= 0) {
		ERRORA("Unable to get a poll descriptors count, error is %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}

	if (err != 1) {				//number of poll descriptors
		DEBUGA_GSMOPEN("Can't handle more than one device\n", GSMOPEN_P_LOG);
		return NULL;
	}

	err = snd_pcm_poll_descriptors(handle, &tech_pvt->pfd, err);
	if (err != 1) {
		ERRORA("snd_pcm_poll_descriptors failed, %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		return NULL;
	}
	DEBUGA_GSMOPEN("Acquired fd %d from the poll descriptor\n", GSMOPEN_P_LOG, tech_pvt->pfd.fd);

	if (stream == SND_PCM_STREAM_CAPTURE) {
		tech_pvt->gsmopen_sound_capt_fd = tech_pvt->pfd.fd;
	}

	state = snd_pcm_state(handle);

	if (state != SND_PCM_STATE_RUNNING) {
		if (state != SND_PCM_STATE_PREPARED) {
			err = snd_pcm_prepare(handle);
			if (err) {
				ERRORA("snd_pcm_prepare failed, %s\n", GSMOPEN_P_LOG, snd_strerror(err));
				return NULL;
			}
			DEBUGA_GSMOPEN("prepared!\n", GSMOPEN_P_LOG);
		}
		if (stream == SND_PCM_STREAM_CAPTURE) {
			err = snd_pcm_start(handle);
			if (err) {
				ERRORA("snd_pcm_start failed, %s\n", GSMOPEN_P_LOG, snd_strerror(err));
				return NULL;
			}
			DEBUGA_GSMOPEN("started!\n", GSMOPEN_P_LOG);
		}
	}
	if (option_debug > 1) {
		snd_output_t *output = NULL;
		err = snd_output_stdio_attach(&output, stdout, 0);
		if (err < 0) {
			ERRORA("snd_output_stdio_attach failed: %s\n", GSMOPEN_P_LOG, snd_strerror(err));
		}
		snd_pcm_dump(handle, output);

#ifndef NO_GSMLIB
		SMSMessageRef sms;
		char content2[1000];
		//sms = SMSMessage::decode("079194710167120004038571F1390099406180904480A0D41631067296EF7390383D07CD622E58CD95CB81D6EF39BDEC66BFE7207A794E2FBB4320AFB82C07E56020A8FC7D9687DBED32285C9F83A06F769A9E5EB340D7B49C3E1FA3C3663A0B24E4CBE76516680A7FCBE920725A5E5ED341F0B21C346D4E41E1BA790E4286DDE4BC0BD42CA3E5207258EE1797E5A0BA9B5E9683C86539685997EBEF61341B249BC966"); // dataCodingScheme = 0
		//sms = SMSMessage::decode("0791934329002000040C9193432766658100009001211133318004D4F29C0E"); // dataCodingScheme = 0
		//sms = SMSMessage::decode("0791934329002000040C919343276665810008900121612521801600CC00E800E900F900F200E00020006300690061006F"); // dataCodingScheme = 8
		sms = SMSMessage::decode("0791934329002000040C919343276665810008900172002293404C006300690061006F0020003100320033002000620065006C00E80020043D043E0432043E044104420438002005DC05E7002005E805D005EA0020FE8EFEE0FEA0FEE4FECBFE9300204EBA5927");	// dataCodingScheme = 8 , text=ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大
		//sms = SMSMessage::decode("07911497941902F00414D0E474989D769F5DE4320839001040122151820000"); // dataCodingScheme = 0
		//NOTICA("SMS=\n%s\n", GSMOPEN_P_LOG, sms->toString().c_str());

		memset(content2, '\0', sizeof(content2));
		if (sms->dataCodingScheme().getAlphabet() == DCS_DEFAULT_ALPHABET) {
			iso_8859_1_to_utf8(tech_pvt, (char *) sms->userData().c_str(), content2, sizeof(content2));
		} else if (sms->dataCodingScheme().getAlphabet() == DCS_SIXTEEN_BIT_ALPHABET) {
			ucs2_to_utf8(tech_pvt, (char *) bufToHex((unsigned char *) sms->userData().data(), sms->userData().length()).c_str(), content2,
						 sizeof(content2));
		} else {
			ERRORA("dataCodingScheme not supported=%d\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());

		}
		//NOTICA("dataCodingScheme=%d\n", GSMOPEN_P_LOG, sms->dataCodingScheme().getAlphabet());
		//NOTICA("userData= |||%s|||\n", GSMOPEN_P_LOG, content2);
#endif// NO_GSMLIB

	}
	if (option_debug > 1)
		DEBUGA_GSMOPEN("ALSA handle = %ld\n", GSMOPEN_P_LOG, (long int) handle);
	return handle;

}

/*! \brief Write audio frames to interface */
#endif /* GSMOPEN_ALSA */

int gsmopen_call(private_t * tech_pvt, char *rdest, int timeout)
{

	//gsmopen_sleep(5000);
	DEBUGA_GSMOPEN("Calling GSM, rdest is: %s\n", GSMOPEN_P_LOG, rdest);
	//gsmopen_signaling_write(tech_pvt, "SET AGC OFF");
	//gsmopen_sleep(10000);
	//gsmopen_signaling_write(tech_pvt, "SET AEC OFF");
	//gsmopen_sleep(10000);

	gsmopen_serial_call(tech_pvt, rdest);
	//ERRORA("failed to communicate with GSM client, now exit\n", GSMOPEN_P_LOG);
	//return -1;
	//}
	return 0;
}


int gsmopen_senddigit(private_t * tech_pvt, char digit)
{

	DEBUGA_GSMOPEN("DIGIT received: %c\n", GSMOPEN_P_LOG, digit);
	if (tech_pvt->controldevprotocol == PROTOCOL_AT && tech_pvt->at_send_dtmf[0]) {
		int res = 0;
		char at_command[256];

		memset(at_command, '\0', 256);
		sprintf(at_command, "%s=\"%c\"", tech_pvt->at_send_dtmf, digit);
		res = gsmopen_serial_write_AT_ack(tech_pvt, at_command);
		if (res) {
			ERRORA("senddigit command failed, command used: '%s=\"%c\"', giving up\n", GSMOPEN_P_LOG, tech_pvt->at_send_dtmf, digit);
		}
	}

	return 0;
}

#ifdef GSMOPEN_ALSA
/*! \brief Write audio frames to interface */
int alsa_write(private_t * tech_pvt, short *data, int datalen)
{
	static char sizbuf[8000];
	static char sizbuf2[16000];
	static char silencebuf[8000];
	static int sizpos = 0;
	int len = sizpos;
	//int pos;
	int res = 0;
	time_t now_timestamp;
	/* size_t frames = 0; */
	snd_pcm_state_t state;
	snd_pcm_sframes_t delayp1=0;
	snd_pcm_sframes_t delayp2=0;

	if(tech_pvt->no_sound==1){
		return res;
	}


	memset(sizbuf, 255, sizeof(sizbuf));
	memset(sizbuf2, 255, sizeof(sizbuf));
	memset(silencebuf, 255, sizeof(sizbuf));

	//ERRORA("data=%p, datalen=%d\n", GSMOPEN_P_LOG, (void *)data, datalen);
	/* We have to digest the frame in 160-byte portions */
	if (datalen > sizeof(sizbuf) - sizpos) {
		ERRORA("Frame too large\n", GSMOPEN_P_LOG);
		res = -1;
	} else {
		memcpy(sizbuf + sizpos, data, datalen);
		memset(data, 255, datalen);
		len += datalen;
		//pos = 0;


#ifdef ALSA_MONITOR
		alsa_monitor_write(sizbuf, len);
#endif
		state = snd_pcm_state(tech_pvt->alsap);
		if (state == SND_PCM_STATE_XRUN) {
			int i;

			DEBUGA_GSMOPEN
				("You've got an ALSA write XRUN in the past (gsmopen can't fill the soundcard buffer fast enough). If this happens often (not after silence or after a pause in the speech, that's OK), and appear to damage the sound quality, first check if you have some IRQ problem, maybe sharing the soundcard IRQ with a broken or heavy loaded ethernet or graphic card. Then consider to increase the alsa_periods_in_buffer (now is set to %d) for this interface in the config file\n",
				 GSMOPEN_P_LOG, tech_pvt->alsa_periods_in_buffer);
			res = snd_pcm_prepare(tech_pvt->alsap);
			if (res) {
				ERRORA("audio play prepare failed: %s\n", GSMOPEN_P_LOG, snd_strerror(res));
			} else {
				res = snd_pcm_format_set_silence(gsmopen_format, silencebuf, len / 2);
				if (res < 0) {
					DEBUGA_GSMOPEN("Silence error %s\n", GSMOPEN_P_LOG, snd_strerror(res));
					res = -1;
				}
				for (i = 0; i < (tech_pvt->alsa_periods_in_buffer - 1); i++) {
					res = snd_pcm_writei(tech_pvt->alsap, silencebuf, len / 2);
					if (res != len / 2) {
						DEBUGA_GSMOPEN("Write returned a different quantity: %d\n", GSMOPEN_P_LOG, res);
						res = -1;
					} else if (res < 0) {
						DEBUGA_GSMOPEN("Write error %s\n", GSMOPEN_P_LOG, snd_strerror(res));
						res = -1;
					}
				}
			}

		}

		res = snd_pcm_delay(tech_pvt->alsap, &delayp1);
		if (res < 0) {
			DEBUGA_GSMOPEN("Error %d on snd_pcm_delay: \"%s\"\n", GSMOPEN_P_LOG, res, snd_strerror(res));
			res = snd_pcm_prepare(tech_pvt->alsap);
			if (res) {
				DEBUGA_GSMOPEN("snd_pcm_prepare failed: '%s'\n", GSMOPEN_P_LOG, snd_strerror(res));
			}
			res = snd_pcm_delay(tech_pvt->alsap, &delayp1);
		}

		delayp2 = snd_pcm_avail_update(tech_pvt->alsap);
		if (delayp2 < 0) {
			DEBUGA_GSMOPEN("Error %d on snd_pcm_avail_update: \"%s\"\n", GSMOPEN_P_LOG, (int) delayp2, snd_strerror(delayp2));

			res = snd_pcm_prepare(tech_pvt->alsap);
			if (res) {
				DEBUGA_GSMOPEN("snd_pcm_prepare failed: '%s'\n", GSMOPEN_P_LOG, snd_strerror(res));
			}
			delayp2 = snd_pcm_avail_update(tech_pvt->alsap);
		}

		if (					/* delayp1 != 0 && delayp1 != 160 */
			   delayp1 < 160 || delayp2 > tech_pvt->alsa_buffer_size) {

			res = snd_pcm_prepare(tech_pvt->alsap);
			if (res) {
				DEBUGA_GSMOPEN
					("snd_pcm_prepare failed while trying to prevent an ALSA write XRUN: %s, delayp1=%d, delayp2=%d\n",
					 GSMOPEN_P_LOG, snd_strerror(res), (int) delayp1, (int) delayp2);
			} else {

				int i;
				for (i = 0; i < (tech_pvt->alsa_periods_in_buffer - 1); i++) {
					res = snd_pcm_format_set_silence(gsmopen_format, silencebuf, len / 2);
					if (res < 0) {
						DEBUGA_GSMOPEN("Silence error %s\n", GSMOPEN_P_LOG, snd_strerror(res));
						res = -1;
					}
					res = snd_pcm_writei(tech_pvt->alsap, silencebuf, len / 2);
					if (res < 0) {
						DEBUGA_GSMOPEN("Write error %s\n", GSMOPEN_P_LOG, snd_strerror(res));
						res = -1;
					} else if (res != len / 2) {
						DEBUGA_GSMOPEN("Write returned a different quantity: %d\n", GSMOPEN_P_LOG, res);
						res = -1;
					}
				}

				DEBUGA_GSMOPEN
					("PREVENTING an ALSA write XRUN (gsmopen can't fill the soundcard buffer fast enough). If this happens often (not after silence or after a pause in the speech, that's OK), and appear to damage the sound quality, first check if you have some IRQ problem, maybe sharing the soundcard IRQ with a broken or heavy loaded ethernet or graphic card. Then consider to increase the alsa_periods_in_buffer (now is set to %d) for this interface in the config file. delayp1=%d, delayp2=%d\n",
					 GSMOPEN_P_LOG, tech_pvt->alsa_periods_in_buffer, (int) delayp1, (int) delayp2);
			}

		}

		memset(sizbuf2, 0, sizeof(sizbuf2));
		if (tech_pvt->alsa_play_is_mono) {
			res = snd_pcm_writei(tech_pvt->alsap, sizbuf, len / 2);
		} else {
			int a = 0;
			int i = 0;
			for (i = 0; i < 8000;) {
				sizbuf2[a] = sizbuf[i];
				a++;
				i++;
				sizbuf2[a] = sizbuf[i];
				a++;
				i--;
				sizbuf2[a] = sizbuf[i];	// comment out this line to use only left 
				a++;
				i++;
				sizbuf2[a] = sizbuf[i];	// comment out this line to use only left
				a++;
				i++;
			}
			res = snd_pcm_writei(tech_pvt->alsap, sizbuf2, len);
		}
		if (res == -EPIPE) {
			DEBUGA_GSMOPEN
				("ALSA write EPIPE (XRUN) (gsmopen can't fill the soundcard buffer fast enough). If this happens often (not after silence or after a pause in the speech, that's OK), and appear to damage the sound quality, first check if you have some IRQ problem, maybe sharing the soundcard IRQ with a broken or heavy loaded ethernet or graphic card. Then consider to increase the alsa_periods_in_buffer (now is set to %d) for this interface in the config file. delayp1=%d, delayp2=%d\n",
				 GSMOPEN_P_LOG, tech_pvt->alsa_periods_in_buffer, (int) delayp1, (int) delayp2);
			res = snd_pcm_prepare(tech_pvt->alsap);
			if (res) {
				ERRORA("audio play prepare failed: %s\n", GSMOPEN_P_LOG, snd_strerror(res));
			} else {

				if (tech_pvt->alsa_play_is_mono) {
					res = snd_pcm_writei(tech_pvt->alsap, sizbuf, len / 2);
				} else {
					int a = 0;
					int i = 0;
					for (i = 0; i < 8000;) {
						sizbuf2[a] = sizbuf[i];
						a++;
						i++;
						sizbuf2[a] = sizbuf[i];
						a++;
						i--;
						sizbuf2[a] = sizbuf[i];
						a++;
						i++;
						sizbuf2[a] = sizbuf[i];
						a++;
						i++;
					}
					res = snd_pcm_writei(tech_pvt->alsap, sizbuf2, len);
				}

			}

		} else {
			if (res == -ESTRPIPE) {
				ERRORA("You've got some big problems\n", GSMOPEN_P_LOG);
			} else if (res == -EAGAIN) {
				DEBUGA_GSMOPEN("Momentarily busy\n", GSMOPEN_P_LOG);
				res = 0;
			} else if (res < 0) {
				ERRORA("Error %d on audio write: \"%s\"\n", GSMOPEN_P_LOG, res, snd_strerror(res));
			}
		}
	}

	if (tech_pvt->audio_play_reset_period) {
		time(&now_timestamp);
		if ((now_timestamp - tech_pvt->audio_play_reset_timestamp) > tech_pvt->audio_play_reset_period) {
			if (option_debug)
				DEBUGA_GSMOPEN("reset audio play\n", GSMOPEN_P_LOG);
			res = snd_pcm_wait(tech_pvt->alsap, 1000);
			if (res < 0) {
				ERRORA("audio play wait failed: %s\n", GSMOPEN_P_LOG, snd_strerror(res));
			}
			res = snd_pcm_drop(tech_pvt->alsap);
			if (res) {
				ERRORA("audio play drop failed: %s\n", GSMOPEN_P_LOG, snd_strerror(res));
			}
			res = snd_pcm_prepare(tech_pvt->alsap);
			if (res) {
				ERRORA("audio play prepare failed: %s\n", GSMOPEN_P_LOG, snd_strerror(res));
			}
			res = snd_pcm_wait(tech_pvt->alsap, 1000);
			if (res < 0) {
				ERRORA("audio play wait failed: %s\n", GSMOPEN_P_LOG, snd_strerror(res));
			}
			time(&tech_pvt->audio_play_reset_timestamp);
		}
	}
	//res = 0;
	//if (res > 0)
	//res = 0;
	return res;
}

#define AST_FRIENDLY_OFFSET 0
int alsa_read(private_t * tech_pvt, short *data, int datalen)
{
	//static struct ast_frame f;
	static short __buf[GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
	static short __buf2[(GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2) * 2];
	short *buf;
	short *buf2;
	static int readpos = 0;
	//static int left = GSMOPEN_FRAME_SIZE;
	static int left;
	snd_pcm_state_t state;
	int r = 0;
	int off = 0;
	int error = 0;
	//time_t now_timestamp;

	//DEBUGA_GSMOPEN("buf=%p, datalen=%d, left=%d\n", GSMOPEN_P_LOG, (void *)buf, datalen, left);
	//memset(&f, 0, sizeof(struct ast_frame)); //giova



	if(tech_pvt->no_sound==1){
		return r;
	}

	left = datalen;


	state = snd_pcm_state(tech_pvt->alsac);
	if (state != SND_PCM_STATE_RUNNING) {
		DEBUGA_GSMOPEN("ALSA read state is not SND_PCM_STATE_RUNNING\n", GSMOPEN_P_LOG);

		if (state != SND_PCM_STATE_PREPARED) {
			error = snd_pcm_prepare(tech_pvt->alsac);
			if (error) {
				ERRORA("snd_pcm_prepare failed, %s\n", GSMOPEN_P_LOG, snd_strerror(error));
				return r;
			}
			DEBUGA_GSMOPEN("prepared!\n", GSMOPEN_P_LOG);
		}
		gsmopen_sleep(1000);
		error = snd_pcm_start(tech_pvt->alsac);
		if (error) {
			ERRORA("snd_pcm_start failed, %s\n", GSMOPEN_P_LOG, snd_strerror(error));
			return r;
		}
		DEBUGA_GSMOPEN("started!\n", GSMOPEN_P_LOG);
		gsmopen_sleep(1000);
	}

	buf = __buf + AST_FRIENDLY_OFFSET / 2;
	buf2 = __buf2 + ((AST_FRIENDLY_OFFSET / 2) * 2);

	if (tech_pvt->alsa_capture_is_mono) {
		r = snd_pcm_readi(tech_pvt->alsac, buf + readpos, left);
		//DEBUGA_GSMOPEN("r=%d, buf=%p, buf+readpos=%p, datalen=%d, left=%d\n", GSMOPEN_P_LOG, r, (void *)buf, (void *)(buf + readpos), datalen, left);
	} else {
		int a = 0;
		int i = 0;
		r = snd_pcm_readi(tech_pvt->alsac, buf2 + (readpos * 2), left);

		for (i = 0; i < (GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2) * 2;) {
			__buf[a] = (__buf2[i] + __buf2[i + 1]) / 2;	//comment out this line to use only left
			//__buf[a] = __buf2[i]; // enable this line to use only left
			a++;
			i++;
			i++;
		}
	}

	if (r == -EPIPE) {
		DEBUGA_GSMOPEN("ALSA XRUN on read\n", GSMOPEN_P_LOG);
		return r;
	} else if (r == -ESTRPIPE) {
		ERRORA("-ESTRPIPE\n", GSMOPEN_P_LOG);
		return r;

	} else if (r == -EAGAIN) {
		int count=0;
		while (r == -EAGAIN) {
			gsmopen_sleep(10000);
			DEBUGA_GSMOPEN("%d ALSA read -EAGAIN, the soundcard is not ready to be read by gsmopen\n", GSMOPEN_P_LOG, count);
			count++;

			if (tech_pvt->alsa_capture_is_mono) {
				r = snd_pcm_readi(tech_pvt->alsac, buf + readpos, left);
			} else {
				int a = 0;
				int i = 0;
				r = snd_pcm_readi(tech_pvt->alsac, buf2 + (readpos * 2), left);

				for (i = 0; i < (GSMOPEN_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2) * 2;) {
					__buf[a] = (__buf2[i] + __buf2[i + 1]) / 2;
					a++;
					i++;
					i++;
				}
			}

		}
	} else if (r < 0) {
		WARNINGA("ALSA Read error: %s\n", GSMOPEN_P_LOG, snd_strerror(r));
	} else if (r >= 0) {
		//DEBUGA_GSMOPEN("read: r=%d, readpos=%d, left=%d, off=%d\n", GSMOPEN_P_LOG, r, readpos, left, off);
		off -= r;				//what is the meaning of this? a leftover, probably
	}
	/* Update positions */
	readpos += r;
	left -= r;

	if (readpos >= GSMOPEN_FRAME_SIZE) {
		int i;
		/* A real frame */
		readpos = 0;
		left = GSMOPEN_FRAME_SIZE;
		for (i = 0; i < r; i++)
			data[i] = buf[i];

	}
	return r;
}

#endif // GSMOPEN_ALSA





int gsmopen_sendsms(private_t * tech_pvt, char *dest, char *text)
{
	//char *idest = data;
	//char rdest[256];
	//private_t *p = NULL;
	//char *device;
	//char *dest;
	//char *text;
	//char *stringp = NULL;
	//int found = 0;
	int failed = 0;
	int err = 0;

	//strncpy(rdest, idest, sizeof(rdest) - 1);
	DEBUGA_GSMOPEN("GSMopenSendsms: dest=%s text=%s\n", GSMOPEN_P_LOG, dest, text);
	DEBUGA_GSMOPEN("START\n", GSMOPEN_P_LOG);
	/* we can use gsmopen_request to get the channel, but gsmopen_request would look for onowned channels, and probably we can send SMSs while a call is ongoing
	 *
	 */

	if (tech_pvt->controldevprotocol != PROTOCOL_AT) {
		ERRORA(", GSMOPEN_P_LOGGSMopenSendsms supports only AT command cellphones at the moment :-( !\n", GSMOPEN_P_LOG);
		return RESULT_FAILURE;
	}

	if (tech_pvt->controldevprotocol == PROTOCOL_AT) {
		char smscommand[16000];
		memset(smscommand, '\0', sizeof(smscommand));

		PUSHA_UNLOCKA(&tech_pvt->controldev_lock);
		LOKKA(tech_pvt->controldev_lock);

		err = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=1");
		if (err) {
			ERRORA("AT+CMGF=1 (set message sending to TEXT (as opposed to PDU)  do not got OK from the phone\n", GSMOPEN_P_LOG);
		}


		if (tech_pvt->no_ucs2) {
			sprintf(smscommand, "AT+CMGS=\"%s\"", dest);	//TODO: support phones that only accept pdu mode
		} else {
			char dest2[1048];

			err = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CSCS=\"UCS2\"");
			if (err) {
				ERRORA("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone\n", GSMOPEN_P_LOG);
			}

			memset(dest2, '\0', sizeof(dest2));
			utf_to_ucs2(tech_pvt, dest, strlen(dest), dest2, sizeof(dest2));
			sprintf(smscommand, "AT+CMGS=\"%s\"", dest2);	//TODO: support phones that only accept pdu mode
		}
		//TODO: support phones that only accept pdu mode
		//TODO would be better to lock controldev here
			//sprintf(smscommand, "AT+CMGS=\"%s\"", dest);	//FIXME: nokia e63 want this
		err = gsmopen_serial_write_AT_noack(tech_pvt, smscommand);
		if (err) {
			ERRORA("Error sending SMS\n", GSMOPEN_P_LOG);
			failed = 1;
			goto uscita;
		}
		err = gsmopen_serial_AT_expect(tech_pvt, "> ", 0, 1);	// wait 1.5s for the prompt, no  crlf
#if 1
		if (err) {
			DEBUGA_GSMOPEN
				("Error or timeout getting prompt '> ' for sending sms directly to the remote party. BTW, seems that we cannot do that with Motorola c350, so we'll write to cellphone memory, then send from memory\n",
				 GSMOPEN_P_LOG);

			err = gsmopen_serial_write_AT_ack(tech_pvt, "ATE1");	//motorola (at least c350) do not echo the '>' prompt when in ATE0... go figure!!!!
			if (err) {
				ERRORA("Error activating echo from modem\n", GSMOPEN_P_LOG);
			}
			tech_pvt->at_cmgw[0] = '\0';
			sprintf(smscommand, "AT+CMGW=\"%s\"", dest);	//TODO: support phones that only accept pdu mode
			err = gsmopen_serial_write_AT_noack(tech_pvt, smscommand);
			if (err) {
				ERRORA("Error writing SMS destination to the cellphone memory\n", GSMOPEN_P_LOG);
				failed = 1;
				goto uscita;
			}
			err = gsmopen_serial_AT_expect(tech_pvt, "> ", 0, 1);	// wait 1.5s for the prompt, no  crlf
			if (err) {
				ERRORA("Error or timeout getting prompt '> ' for writing sms text in cellphone memory\n", GSMOPEN_P_LOG);
				failed = 1;
				goto uscita;
			}
		}
#endif

		//sprintf(text,"ciao 123 belè новости לק ראת ﺎﻠﺠﻤﻋﺓ 人大"); //let's test the beauty of utf
		memset(smscommand, '\0', sizeof(smscommand));
		if (tech_pvt->no_ucs2) {
			sprintf(smscommand, "%s", text);
		} else {
			utf_to_ucs2(tech_pvt, text, strlen(text), smscommand, sizeof(smscommand));
		}

		smscommand[strlen(smscommand)] = 0x1A;
		DEBUGA_GSMOPEN("smscommand len is: %d, text is:|||%s|||\n", GSMOPEN_P_LOG, (int) strlen(smscommand), smscommand);

		err = gsmopen_serial_write_AT_ack_nocr_longtime(tech_pvt, smscommand);
		//TODO would be better to unlock controldev here
		if (err) {
			ERRORA("Error writing SMS text to the cellphone memory\n", GSMOPEN_P_LOG);
			//return RESULT_FAILURE;
			failed = 1;
			goto uscita;
		}
		if (tech_pvt->at_cmgw[0]) {
			sprintf(smscommand, "AT+CMSS=%s", tech_pvt->at_cmgw);
			err = gsmopen_serial_write_AT_expect_longtime(tech_pvt, smscommand, "OK");
			if (err) {
				ERRORA("Error sending SMS from the cellphone memory\n", GSMOPEN_P_LOG);
				//return RESULT_FAILURE;
				failed = 1;
				goto uscita;
			}

			err = gsmopen_serial_write_AT_ack(tech_pvt, "ATE0");	//motorola (at least c350) do not echo the '>' prompt when in ATE0... go figure!!!!
			if (err) {
				ERRORA("Error de-activating echo from modem\n", GSMOPEN_P_LOG);
			}
		}
	  uscita:
		gsmopen_sleep(1000);

		if (tech_pvt->at_cmgw[0]) {

			/* let's see what we've sent, just for check TODO: Motorola it's not reliable! Motorola c350 tells that all was sent, but is not true! It just sends how much it fits into one SMS FIXME: need an algorithm to calculate how many ucs2 chars fits into an SMS. It make difference based, probably, on the GSM alphabet translation, or so */
			sprintf(smscommand, "AT+CMGR=%s", tech_pvt->at_cmgw);
			err = gsmopen_serial_write_AT_ack(tech_pvt, smscommand);
			if (err) {
				ERRORA("Error reading SMS back from the cellphone memory\n", GSMOPEN_P_LOG);
			}

			/* let's delete from cellphone memory what we've sent */
			sprintf(smscommand, "AT+CMGD=%s", tech_pvt->at_cmgw);
			err = gsmopen_serial_write_AT_ack(tech_pvt, smscommand);
			if (err) {
				ERRORA("Error deleting SMS from the cellphone memory\n", GSMOPEN_P_LOG);
			}

			tech_pvt->at_cmgw[0] = '\0';
		}
		//gsmopen_sleep(500000);             //.5 secs
		UNLOCKA(tech_pvt->controldev_lock);
		POPPA_UNLOCKA(&tech_pvt->controldev_lock);
	}

	err = gsmopen_serial_write_AT_ack(tech_pvt, "AT+CMGF=0");
	if (err) {
		DEBUGA_GSMOPEN("AT+CMGF=0 (set message sending to PDU (as opposed to TEXT)  do not got OK from the phone, continuing\n", GSMOPEN_P_LOG);
	}


	DEBUGA_GSMOPEN("FINISH\n", GSMOPEN_P_LOG);
	if (failed)
		return -1;
	else
		return RESULT_SUCCESS;
}

/************************************************/

/* LUIGI RIZZO's magic */
/* boost support. BOOST_SCALE * 10 ^(BOOST_MAX/20) must
 * be representable in 16 bits to avoid overflows.
 */
#define BOOST_SCALE     (1<<9)
#define BOOST_MAX       40		/* slightly less than 7 bits */

/*
 * store the boost factor
 */
void gsmopen_store_boost(char *s, double *boost)
{
	private_t *tech_pvt = NULL;

	if (sscanf(s, "%lf", boost) != 1) {
		ERRORA("invalid boost <%s>\n", GSMOPEN_P_LOG, s);
		return;
	}
	if (*boost < -BOOST_MAX) {
		WARNINGA("boost %s too small, using %d\n", GSMOPEN_P_LOG, s, -BOOST_MAX);
		*boost = -BOOST_MAX;
	} else if (*boost > BOOST_MAX) {
		WARNINGA("boost %s too large, using %d\n", GSMOPEN_P_LOG, s, BOOST_MAX);
		*boost = BOOST_MAX;
	}
#ifdef WIN32
	*boost = exp(log ((double)10) * *boost / 20) * BOOST_SCALE;
#else
	*boost = exp(log(10) * *boost / 20) * BOOST_SCALE;
#endif //WIN32
	if (option_debug > 1)
		DEBUGA_GSMOPEN("setting boost %s to %f\n", GSMOPEN_P_LOG, s, *boost);
}


int gsmopen_sound_boost(void *data, int samples_num, double boost)
{
/* LUIGI RIZZO's magic */
	if (boost != 0 && (boost < 511 || boost > 513)) {		/* scale and clip values */
		int i, x;

		int16_t *ptr = (int16_t *) data;

		for (i = 0; i < samples_num; i++) {
			x = (int) (ptr[i] * boost) / BOOST_SCALE;
			if (x > 32767) {
				x = 32767;
			} else if (x < -32768) {
				x = -32768;
			}
			ptr[i] = x;
		}
	} else {
		//printf("BOOST=%f\n", boost);
	}

	return 0;
}


int gsmopen_serial_getstatus_AT(private_t * tech_pvt)
{
	int res;
	private_t *p = tech_pvt;

#if 0
	if (p->owner) {
		if (p->owner->_state != AST_STATE_UP && p->owner->_state != AST_STATE_DOWN) {
			DEBUGA_AT("No getstatus, we're neither UP nor DOWN\n", GSMOPEN_P_LOG);
			return 0;
		}
	}
#endif


	PUSHA_UNLOCKA(p->controldev_lock);
	LOKKA(p->controldev_lock);
	res = gsmopen_serial_write_AT_ack(p, "AT");
	if (res) {
		ERRORA("AT was not acknowledged, continuing but maybe there is a problem\n", GSMOPEN_P_LOG);
	}
	gsmopen_sleep(1000);

	if (strlen(p->at_query_battchg)) {
		res = gsmopen_serial_write_AT_expect(p, p->at_query_battchg, p->at_query_battchg_expect);
		if (res) {
			WARNINGA("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, p->at_query_battchg, p->at_query_battchg_expect);
		}
		gsmopen_sleep(1000);
	}

	if (strlen(p->at_query_signal)) {
		res = gsmopen_serial_write_AT_expect(p, p->at_query_signal, p->at_query_signal_expect);
		if (res) {
			WARNINGA("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, p->at_query_signal, p->at_query_signal_expect);
		}
		gsmopen_sleep(1000);
	}

	if (!p->network_creg_not_supported) {
		res = gsmopen_serial_write_AT_ack(p, "AT+CREG?");
		if (res) {
			WARNINGA("%s does not get %s from the phone. Continuing.\n", GSMOPEN_P_LOG, "AT+CREG?", "OK");
		}
		gsmopen_sleep(1000);
	}

	//FIXME all the following commands in config!

	if (p->sms_cnmi_not_supported) {
		res = gsmopen_serial_write_AT_ack(p, "AT+MMGL=\"HEADER ONLY\"");
		if (res) {
			WARNINGA
				("%s does not get %s from the phone. If your phone is not Motorola, please contact the gsmopen developers. Else, if your phone IS a Motorola, probably a long msg was incoming and ther first part was read and then deleted. The second part is now orphan. If you got this warning  repeatedly, and you cannot correctly receive SMSs from this interface, please manually clean all messages (and the residual parts of them) from the cellphone/SIM. Continuing.\n",
				 GSMOPEN_P_LOG, "AT+MMGL=\"HEADER ONLY\"", "OK");
		} else {
			gsmopen_sleep(1000);
			if (p->unread_sms_msg_id) {
				char at_command[256];

				res = gsmopen_serial_write_AT_ack(p, "AT+CSCS=\"UCS2\"");
				if (res) {
					ERRORA("AT+CSCS=\"UCS2\" (set TE messages to ucs2)  do not got OK from the phone\n", GSMOPEN_P_LOG);
					memset(p->sms_message, 0, sizeof(p->sms_message));
				}

				memset(at_command, 0, sizeof(at_command));
				sprintf(at_command, "AT+CMGR=%d", p->unread_sms_msg_id);
				memset(p->sms_message, 0, sizeof(p->sms_message));

				p->reading_sms_msg = 1;
				res = gsmopen_serial_write_AT_ack(p, at_command);
				p->reading_sms_msg = 0;
				if (res) {
					ERRORA("AT+CMGR (read SMS) do not got OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
				}
				res = gsmopen_serial_write_AT_ack(p, "AT+CSCS=\"GSM\"");
				if (res) {
					ERRORA("AT+CSCS=\"GSM\" (set TE messages to GSM) do not got OK from the phone\n", GSMOPEN_P_LOG);
				}
				memset(at_command, 0, sizeof(at_command));
				sprintf(at_command, "AT+CMGD=%d", p->unread_sms_msg_id);	/* delete the message */
				p->unread_sms_msg_id = 0;
				res = gsmopen_serial_write_AT_ack(p, at_command);
				if (res) {
					ERRORA("AT+CMGD (Delete SMS) do not got OK from the phone, message sent was:|||%s|||\n", GSMOPEN_P_LOG, at_command);
				}

				if (strlen(p->sms_message)) {
#if 0

					manager_event(EVENT_FLAG_SYSTEM, "GSMOPENincomingsms", "Interface: %s\r\nSMS_Message: %s\r\n", p->name, p->sms_message);

					if (strlen(p->sms_receiving_program)) {
						int fd1[2];
						pid_t pid1;
						char *arg1[] = { p->sms_receiving_program, (char *) NULL };
						int i;

						DEBUGA_AT("incoming SMS message:---%s---\n", GSMOPEN_P_LOG, p->sms_message);
						pipe(fd1);
						pid1 = fork();

						if (pid1 == 0) {	//child
							int err;

							dup2(fd1[0], 0);	// Connect stdin to pipe output
							close(fd1[1]);	// close input pipe side
							setsid();	//session id
							err = execvp(arg1[0], arg1);	//exec our program, with stdin connected to pipe output
							if (err) {
								ERRORA
									("'sms_receiving_program' is set in config file to '%s', and it gave us back this error: %d, (%s). SMS received was:---%s---\n",
									 GSMOPEN_P_LOG, p->sms_receiving_program, err, strerror(errno), p->sms_message);
							}
							close(fd1[0]);	// close output pipe side
						}		//starting here continue the parent
						close(fd1[0]);	// close output pipe side
						// write the msg on the pipe input
						for (i = 0; i < strlen(p->sms_message); i++) {
							write(fd1[1], &p->sms_message[i], 1);
						}
						close(fd1[1]);	// close pipe input, let our program know we've finished
					} else {
						ERRORA
							("got SMS incoming message, but 'sms_receiving_program' is not set in config file. SMS received was:---%s---\n",
							 GSMOPEN_P_LOG, p->sms_message);
					}
#endif //0
					DEBUGA_GSMOPEN("got SMS incoming message. SMS received was:---%s---\n", GSMOPEN_P_LOG, p->sms_message);
				}
#if 0							//is this one needed? maybe it can interrupt an incoming call that is just to announce itself
				if (p->phone_callflow == CALLFLOW_CALL_IDLE && p->interface_state == AST_STATE_DOWN && p->owner == NULL) {
					/* we're not in a call, neither calling */
					res = gsmopen_serial_write_AT_ack(p, "AT+CKPD=\"EEE\"");
					if (res) {
						ERRORA("AT+CKPD=\"EEE\" (cellphone screen back to user) do not got OK from the phone\n", GSMOPEN_P_LOG);
					}
				}
#endif
			}
		}
	}

	UNLOCKA(p->controldev_lock);
	POPPA_UNLOCKA(p->controldev_lock);
	return 0;
}
