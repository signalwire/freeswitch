#ifndef LIBCTB_LINUX_SERPORT_H_INCLUDED_
#define LIBCTB_LINUX_SERPORT_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        linux/serport.h
// Purpose:
// Author:      Joachim Buermann
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "ctb-0.16/serportx.h"
#include <linux/serial.h>
#include <termios.h>

namespace ctb {

/*!
  \class SerialPort
   
  \brief the linux version
*/
    class SerialPort : public SerialPort_x
    {
    protected:
	   /*!
		\brief under Linux, the serial ports are normal file descriptor
	   */
	   int fd;
	   /*!
		\brief Linux defines this struct termios for controling asynchronous
		communication. t covered the active settings, save_t the original 
		settings.
	   */
	   struct termios t, save_t;

	   /*!
		\brief The Linux serial driver summing all breaks, framings, overruns
		and parity errors for each port during system runtime. Because we only
		need the errors during a active connection, we must save the actual
		error numbers in this separate structurs.
	   */
	   struct serial_icounter_struct save_info, last_info;

	   /*!
		\brief adaptor member function, to convert the plattform independent
		type wxBaud into a linux conform value.
		\param baud the baudrate as wxBaud type
		\return speed_t linux specific data type, defined in termios.h
	   */
	   speed_t AdaptBaudrate( int baud );
    
	   int CloseDevice();
	   int OpenDevice(const char* devname, void* dcs);

	   /*!
		\brief internal member function to set an unusal (non-standard)
		baudrate. Called by SetBaudrate.
	    */
	   int SetBaudrateAny( int baudrate );

	   /*!
		\brief internal member function to set a standard baudrate.
		Called by SetBaudrate.
	    */
	   int SetBaudrateStandard( int baudrate );

    public:
	   SerialPort();

	   ~SerialPort();

	   int ChangeLineState( SerialLineState flags );

	   int ClrLineState( SerialLineState flags );

	   int GetLineState();
	   int Ioctl(int cmd,void* args);
	   int IsOpen();
	   int Read(char* buf,size_t len);
	   int SendBreak(int duration);

	   int SetBaudrate( int baudrate );

	   int SetParityBit( bool parity );

	   int SetLineState( SerialLineState flags );

	   int Write(char* buf,size_t len);
    };

} // namespace ctb

#endif
