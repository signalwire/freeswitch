#ifndef LIBCTB_WIN32_TIMER_H_INCLUDED_
#define LIBCTB_WIN32_TIMER_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        win32/serport.h
// Purpose:
// Author:      Joachim Buermann
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "../serportx.h"
#include <windows.h>

namespace ctb {

    /*!
	 \class SerialPort
   
	 \brief the win32 version
    */
    class SerialPort : public SerialPort_x
    {
    protected:
	   /*!
		\brief the win32 api equivalent for the Linux
		file descriptor
	   */
	   HANDLE fd;
	   /*!
		\brief a very special struct in the win32 api for controling
		an asynchronous serial communication through the com ports.
	   */
	   OVERLAPPED m_ov;

	   /*!
		\brief The win32 API only allows to test for an existing
		break, framing, overrun or parity, not for the occured numbers.
		So every error event decrement this internal error struct and
		can request by an Ioctl call.
	   */
	   SerialPort_EINFO einfo;
    
	   /*!
		\brief The win32 API doesn't have any function to detect the
		current state of the output lines RST and DTR (if someone knows
		some function, please contact me).
		So we save the state always after changing one or both lines
		(for example, on Open, SetLineState and ChangeLineState)
	   */
	   int m_rtsdtr_state;

	   int CloseDevice();
	   int OpenDevice(const char* devname, void* dcs);
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
	   int SetLineState( SerialLineState flags );
	   int SetParityBit( bool parity );
	   int Write(char* buf,size_t len);
    };

} // namespace ctb

#endif
