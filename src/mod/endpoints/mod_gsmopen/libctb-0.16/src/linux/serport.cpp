/////////////////////////////////////////////////////////////////////////////
// Name:        linux/serport.cpp
// Purpose:
// Author:      Joachim Buermann
// Id:          $Id: serport.cpp,v 1.1.1.1 2004/11/24 10:30:11 jb Exp $
// Copyright:   (c) 2001 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "ctb-0.16/linux/serport.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define CMSPAR	  010000000000		/* mark or space (stick) parity */

namespace ctb {

    const char* COM1 = "/dev/ttyS0";
    const char* COM2 = "/dev/ttyS1";
    const char* COM3 = "/dev/ttyS2";
    const char* COM4 = "/dev/ttyS3";
    const char* COM5 = "/dev/ttyS4";
    const char* COM6 = "/dev/ttyS5";
    const char* COM7 = "/dev/ttyS6";
    const char* COM8 = "/dev/ttyS7";
    const char* COM9 = "/dev/ttyS8";
    const char* COM10 = "/dev/ttyS9";
    const char* COM11= "/dev/ttyS10";
    const char* COM12= "/dev/ttyS11";
    const char* COM13 = "/dev/ttyS12";
    const char* COM14 = "/dev/ttyS13";
    const char* COM15 = "/dev/ttyS14";
    const char* COM16 = "/dev/ttyS15";
    const char* COM17 = "/dev/ttyS16";
    const char* COM18 = "/dev/ttyS17";
    const char* COM19 = "/dev/ttyS18";
    const char* COM20 = "/dev/ttyS19";

    SerialPort::SerialPort() :
	   SerialPort_x()
    {
	   fd = -1;
    };

    SerialPort::~SerialPort()
    {
	   Close();
    };

    speed_t SerialPort::AdaptBaudrate( int baud )
    {
	   switch(baud) {
	   case 150: return B150;
	   case 300: return B300;
	   case 600: return B600;
	   case 1200: return B1200;
	   case 2400: return B2400;
	   case 4800: return B4800;
	   case 9600: return B9600;
	   case 19200: return B19200;
	   case 57600: return B57600;
	   case 115200: return B115200;
	   case 230400: return B230400;
	   case 460800: return B460800;
	   case 921600: return B921600;

		  // NOTE! The speed of 38400 is required, if you want to set
		  //       an non-standard baudrate. See below!
	   default: return B38400;
	   }
    };

    int SerialPort::CloseDevice()
    {
	   int err = 0;
	   // only close an open file handle
	   if(fd < 0) return EBADF;
	   // With some systems, it is recommended to flush the serial port's 
	   // Output before closing it, in order to avoid a possible hang of
	   // the process...
	   // Thanks to Germain (I couldn't answer you, because your email
	   // address was invalid)
	   tcflush(fd, TCOFLUSH);

	   // Don't recover the orgin settings while the device is open. This
	   // implicate a mismatched data output!
	   // Just close device
	   err = close( fd );

	   fd = -1;

	   return err;
    };

    int SerialPort::ChangeLineState( SerialLineState flags )
    {
	   int state;
	   ioctl(fd,TIOCMGET,&state);
	   state ^= flags;
	   return ioctl(fd,TIOCMSET,&state);
    };

    int SerialPort::ClrLineState( SerialLineState flags )
    {
	   return ioctl(fd,TIOCMBIC,&flags);
    };

    int SerialPort::GetLineState()
    {
	   SerialLineState flags = LinestateNull;

	   if( ioctl( fd, TIOCMGET, &flags ) < 0 ) {

		  return -1;

	   }
	   return (int)( flags & 0x1FF );
    };

//
// included from /usr/include/linux/serial.h
//
//  struct serial_icounter_struct {
//  	int cts, dsr, rng, dcd;
//  	int rx, tx;
//  	int frame, overrun, parity, brk;
//  	int buf_overrun;
//  	int reserved[9];
//  };
//
    int SerialPort::Ioctl(int cmd, void* args)
    {
	   int count = 0;
	   int err = 0;
	   struct serial_icounter_struct info;
	   SerialPort_EINFO einfo;

	   switch(cmd) {
	   case CTB_RESET:
		  return SendBreak(0);
	   case CTB_SER_GETEINFO:
		  err = ioctl(fd,TIOCGICOUNT,&info);
		  if(err) return err;
		  einfo.brk = info.brk - save_info.brk;
		  einfo.frame = info.frame - save_info.frame;
		  einfo.overrun = info.overrun - save_info.overrun;
		  einfo.parity = info.parity - save_info.parity;
		  *(SerialPort_EINFO*)args = einfo;
		  break;
	   case CTB_SER_GETBRK:
		  err = ioctl(fd,TIOCGICOUNT,&info);
		  if(err) return err;
		  if(last_info.brk != info.brk) count = 1;
		  break;
	   case CTB_SER_GETFRM:
		  err = ioctl(fd,TIOCGICOUNT,&info);
		  if(err) return err;
		  if(last_info.frame != info.frame) count = 1;
		  break;
	   case CTB_SER_GETOVR:
		  err = ioctl(fd,TIOCGICOUNT,&info);
		  if(err) return err;
		  if(last_info.overrun != info.overrun) count = 1;
		  break;
	   case CTB_SER_GETPAR:
		  err = ioctl(fd,TIOCGICOUNT,&info);
		  if(err) return err;
		  if(last_info.parity != info.parity) count = 1;
		  break;
	   case CTB_SER_GETINQUE:
		  err = ioctl(fd,TIOCINQ,&count);
		  if(err) return err;
		  *(int*)args = count;
		  return 0;
	   case CTB_SER_SETPAR:
		  return SetParityBit( *(int*)args == 1 ); 
	   default:
		  return -1;
	   }
	   last_info = info;
	   return 0;
    };

    int SerialPort::IsOpen()
    {
	   return (fd != -1);
    };

    int SerialPort::OpenDevice(const char* devname, void* dcs)
    {
	   // if dcs isn't NULL, type cast
	   if(dcs) m_dcs = *(SerialPort_DCS*)dcs;
	   // open serial comport device for reading and writing,
	   // don't wait (O_NONBLOCK)
	   fd = open(devname, O_RDWR | O_NOCTTY | O_NONBLOCK);
	   if(fd >= 0) {

		  // exclusive use
		  int dummy;

		  ioctl( fd, TIOCEXCL, &dummy );

		  tcgetattr(fd,&t);
		  save_t = t;

		  // save the device name
		  strncpy(m_devname,(char*)devname,sizeof(m_devname));
		  // we write an eos to avoid a buffer overflow
		  m_devname[sizeof(m_devname)-1] = '\0';

		  // Fill the internal terios struct.
		  // If the given baudrate is an non-standard one, the AdaptBaudrate
		  // call returns the linux specific value B38400 which is a
		  // condition for the later switch to an unusual baudrate!
		  cfsetspeed(&t, AdaptBaudrate( m_dcs.baud ) );

		  //cfsetospeed(&t, AdaptBaudrate( m_dcs.baud ) );

		  // parity settings
		  switch( m_dcs.parity ) {

		  case ParityNone:
			 t.c_cflag &= ~PARENB; break;

		  case ParityOdd:
			 t.c_cflag |= PARENB;
			 t.c_cflag |= PARODD;
			 break;

		  case ParityEven:
			 t.c_cflag |= PARENB;
			 t.c_cflag &= ~PARODD;
			 break;

		  case ParityMark:
			 t.c_cflag |= PARENB | CMSPAR | PARODD;
			 break;

		  case ParitySpace:
			 t.c_cflag |= PARENB | CMSPAR;
			 t.c_cflag &= ~PARODD;
			 break;
		  }
		  
		  // stopbits
		  if(m_dcs.stopbits == 2)
			 t.c_cflag |= CSTOPB;
		  else
			 t.c_cflag &= ~CSTOPB;
		  // wordlen
		  t.c_cflag &= ~CSIZE;
		  if(m_dcs.wordlen == 7) t.c_cflag |= CS7;
		  else if(m_dcs.wordlen == 6) t.c_cflag |= CS6;
		  else if(m_dcs.wordlen == 5) t.c_cflag |= CS5;
		  // this is the default
		  else t.c_cflag |= CS8;
		  // rts/cts
		  if(m_dcs.rtscts == false)
			 t.c_cflag &= ~CRTSCTS;
		  else
			 t.c_cflag |= CRTSCTS;

		  t.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
		  t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF | IXANY);
		  t.c_iflag |= IGNPAR;
		  t.c_oflag &= ~OPOST;

		  if(m_dcs.xonxoff == true) {
			 t.c_iflag |= (IXON | IXOFF);
		  }

		  // look out!
		  // MIN = 1 means, in TIME (1/10 secs) defined timeout
		  // will be started AFTER receiving the first byte
		  // so we must set MIN = 0. (timeout starts immediately, abort
		  // also without readed byte)
		  t.c_cc[VMIN] = 0;
		  // timeout in 1/10 secs
		  // no timeout for non blocked transfer
		  t.c_cc[VTIME] = 0;
		  // write the settings
		  tcsetattr(fd,TCSANOW,&t);
		  // it's careless, but in the moment we don't test
		  // the return of tcsetattr (normally there is no error)
	
		  // request the actual numbers of breaks, framing, overrun
		  // and parity errors (because Linux summing all of them during
		  // system lifetime, not only while serial port is open.
		  ioctl(fd,TIOCGICOUNT,&save_info);
		  // it's also careless, but we assume, that there was no error
		  last_info = save_info;

		  // in case of a non-standard rate, the termios struct have to set
		  // with the B38400 rate, see above!
		  if( ! IsStandardRate( m_dcs.baud ) ) {

			 SetBaudrateAny( m_dcs.baud );

		  }
	   }
	   return fd;
    };

    int SerialPort::Read(char* buf,size_t len)
    {
	   if(m_fifo->items() > 0) {
		  return m_fifo->read(buf,len);
	   }
	   // Read() (using read() ) will return an 'error' EAGAIN as it is 
	   // set to non-blocking. This is not a true error within the 
	   // functionality of Read, and thus should be handled by the caller.
	   int n = read(fd,buf,len);
	   if((n < 0) && (errno == EAGAIN)) return 0;
	   return n;
    };

    int SerialPort::SendBreak(int duration)
    {
	   // the parameter is equal with linux
	   return tcsendbreak(fd,duration);
    };

    /*
	 Note: The following hints are copied from the ftdi_sio.c sources of
	 the kernel modul for the USB to RS232 converter using an FTDI
	 chipset (FT232BM or similar). Thanks to all those people
	 contribute code and above all helpful comments to this modul.
    */

    /*
	* The logic involved in setting the baudrate can be cleanly split in 3 steps.
	* Obtaining the actual baud rate is a little tricky since unix traditionally
	* somehow ignored the possibility to set non-standard baud rates.
	* 1. Standard baud rates are set in tty->termios->c_cflag
	* 2. If these are not enough, you can set any speed using alt_speed as 
	* follows:
	*    - set tty->termios->c_cflag speed to B38400
	*    - set your real speed in tty->alt_speed; it gets ignored when
	*      alt_speed==0, (or)
	*    - call TIOCSSERIAL ioctl with (struct serial_struct) set as follows:
	*      flags & ASYNC_SPD_MASK == ASYNC_SPD_[HI, VHI, SHI, WARP], this just
	*      sets alt_speed to (HI: 57600, VHI: 115200, SHI: 230400, WARP: 460800)
	* ** Steps 1, 2 are done courtesy of tty_get_baud_rate
	* 3. You can also set baud rate by setting custom divisor as follows
	*    - set tty->termios->c_cflag speed to B38400
	*    - call TIOCSSERIAL ioctl with (struct serial_struct) set as follows:
	*      o flags & ASYNC_SPD_MASK == ASYNC_SPD_CUST
	*      o custom_divisor set to baud_base / your_new_baudrate
	* ** Step 3 is done courtesy of code borrowed from serial.c - I should really
	*    spend some time and separate+move this common code to serial.c, it is
	*    replicated in nearly every serial driver you see.
    */

    int SerialPort::SetBaudrateAny( int baudrate )
    {
	   struct serial_struct ser_info; 

	   int result = ioctl( fd, TIOCGSERIAL, &ser_info ); 

	   ser_info.flags = ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;

	   ser_info.custom_divisor = ser_info.baud_base / baudrate; 

	   result = ioctl( fd, TIOCSSERIAL, &ser_info );

	   return result;
    }

    int SerialPort::SetBaudrateStandard( int baudrate )
    {
	   speed_t baud = AdaptBaudrate( baudrate );
	   // setting the input baudrate
	   if(cfsetspeed(&t,baud) < 0) {
		  return -1;
	   }
	   // take over
	   m_dcs.baud = baudrate;

	   tcsetattr(fd,TCSANOW,&t);

	   return tcgetattr( fd, &t );
	   
    };

    int SerialPort::SetBaudrate( int baudrate )
    {
	   return IsStandardRate( baudrate ) ?
		  SetBaudrateStandard( baudrate ) :
		  SetBaudrateAny( baudrate );

    }

    int SerialPort::SetLineState( SerialLineState flags )
    {
	   return ioctl(fd,TIOCMBIS,&flags);
    };

    int SerialPort::SetParityBit( bool parity )
    {
	   // waits until all output has been transmitted
	   tcdrain( fd );

	   // now read the current termios settings and manipulate
	   // the parity
	   tcgetattr( fd, &t );

	   if( parity ) { 
		  
		  t.c_cflag |= PARENB | CMSPAR | PARODD;

	   }
	   else {

		  t.c_cflag |= PARENB | CMSPAR;

		  t.c_cflag &= ~PARODD;

	   }		  

	   tcsetattr( fd,TCSANOW, &t );

	   tcgetattr( fd, &t );

	   return 0;
    }

    int SerialPort::Write(char* buf,size_t len)
    {
	   // Write() (using write() ) will return an 'error' EAGAIN as it is 
	   // set to non-blocking. This is not a true error within the 
	   // functionality of Write, and thus should be handled by the caller.
	   int n = write(fd,buf,len);
	   if((n < 0) && (errno == EAGAIN)) return 0;
	   return n;
    };

} // namespace ctb
