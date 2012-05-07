#ifndef LIBCTB_IOBASE_H_INCLUDED_
#define LIBCTB_IOBASE_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        iobase.h
// Purpose:     io basic class
// Author:      Joachim Buermann
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "ctb-0.16/fifo.h"
#include <sys/types.h>

/**
   \file iobase.h
*/

namespace ctb {

    enum {

	   CTB_COMMON = 0x0000,
	   CTB_SERIAL = 0x0100,
	   CTB_GPIB = 0x0200,
	   CTB_TIMEOUT_INFINITY = 0xFFFFFFFF
    };

//    const unsigned int wxTIMEOUT_INFINITY = 0xFFFFFFFF; 

/*!
  \enum IOBaseIoctls

  Defines the ioctl calls for derivated classes. The following Ioctl 
  calls are valid for all from wxIOBase derivated classes.
*/
    enum IOBaseIoctls {
	   /*!
		Reset the connected device. For a serial (RS232) connection,
		a break is send. For GPIB the IFC (Interface Clear) line is
		set.
	   */
	   CTB_RESET = CTB_COMMON
    };

/*!
  \class IOBase
  An abstract class for different interfaces. The idea behind this:
  Similar to the virtual file system this class defines a lot of
  preset member functions, which the derivate classes must be
  overload.
  In the main thing these are: open a interface (such as RS232),
  reading and writing non blocked through the interface and at
  last, close it.
  For special interface settings the method ioctl was defined.
  (control interface). ioctl covers some interface dependent
  settings like switch on/off the RS232 status lines and must also
  be defined from each derivated class.
*/ 

    class IOBase
    {
    protected:
	   /*!
		\brief internal fifo (first in, first out queue) to put back
		already readed bytes into the reading stream. After put back a single
		byte or sequence of characters, you can read them again with the
		next Read call.
	   */
	   Fifo* m_fifo;
	   enum {
		  /// fifosize of the putback fifo 
		  fifoSize = 256
	   };
	   /*!
		Close the interface (internally the file descriptor, which was
		connected with the interface).
		\return zero on success, otherwise -1.
	   */
	   virtual int CloseDevice() = 0;
	   /*!
		Open the interface (internally to request a file descriptor for the
		given interface). The second parameter is a undefined pointer of a
		device dependent data struct. It must be undefined, because different
		devices have different settings. A serial device like the com ports
		points here to a data struct, includes information like baudrate,
		parity, count of stopbits and wordlen and so on. Another devices
		(for example a IEEE) needs a adress and EOS (end of string character)
		and don't use baudrate or parity.
		\param devname the name of the device, presents the given interface.
		Under windows for example COM1, under Linux /dev/cua0. Use wxCOMn to
		avoid plattform depended code (n is the serial port number, beginning
		with 1).
		\param dcs untyped pointer of advanced device parameters,
		\sa struct dcs_devCUA (data struct for the serail com ports)
		\return zero on success, otherwise -1
	   */
	   virtual int OpenDevice(const char* devname, void* dcs = 0L) = 0;
    public:
	   /*!
		Default constructor
	   */
	   IOBase() {
		  m_fifo = new Fifo(fifoSize);
	   };

	   /*!
		Default destructor
	   */
	   virtual ~IOBase() {
		  delete m_fifo;
	   };
	   /*!
		\brief A little helper function to detect the class name
		\return the name of the class
	   */
	   virtual const char* ClassName() {return "ctb::IOBase";};
	   /*!
		Closed the interface. Internally it calls the CloseDevice()
		method, which must be defined in the derivated class.
		\return zero on success, or -1 if an error occurred.
	   */
	   int Close() {return CloseDevice();};

	   /*!
		In this method we can do all things, which are different
		between the discrete interfaces. The method is similar to the
		C ioctl function. We take a command number and a integer
		pointer as command parameter.
		An example for this is the reset of a connection between a PC
		and one ore more other instruments. On serial (RS232) connections
		mostly a break will be send, GPIB on the other hand defines a
		special line on the GPIB bus, to reset all connected devices.
		If you only want to reset your connection, you should use the
		Ioctl methode for doing this, independent of the real type of
		the connection.
		\param cmd a command identifier, (under Posix such as TIOCMBIS
		for RS232 interfaces), IOBaseIoctls
		\param args typeless parameter pointer for the command above.
		\return zero on success, or -1 if an error occurred.
	   */    
	   virtual int Ioctl(int cmd,void* args) {return -1;};

	   /*!
		Returns the current state of the device.
		\return 1 if device is valid and open, otherwise 0
	   */
	   virtual int IsOpen() = 0;

	   /*!
		\param devname name of the interface, we want to open
		\param dcs a untyped pointer to a device control struct. If
		he is NULL, the default device parameter will be used.
		\return the new file descriptor, or -1 if an error occurred

		The pointer dcs will be used for special device dependent
		settings. Because this is very specific, the struct or
		destination of the pointer will be defined by every device
		itself. (For example: a serial device class should refer
		things like parity, word length and count of stop bits,
		a IEEE class adress and EOS character).
	   */
	   int Open(const char* devname,void* dcs=0L) {
		  return OpenDevice(devname,dcs);
	   };

	   /*!
		\brief In some circumstances you want to put back a already
		readed byte (for instance, you have overreaded it and like to
		parse the recieving bytes again).
		The internal fifo stores fifoSize characters until you
		have to read again.
		\param ch the character to put back in the input stream
		\return 1, if successful, otherwise 0
	   */
	   int PutBack(char ch) {
		  return m_fifo->put(ch);
	   };

	   /*!
		Read attempt to read len bytes from the interface into the buffer
		starting with buf. Read never blocks. If there are no bytes for 
		reading, Read returns zero otherwise the count of bytes been readed.
		\param buf starting adress of the buffer
		\param len count of bytes, we want to read
		\return -1 on fails, otherwise the count of readed bytes
	   */
	   virtual int Read(char* buf,size_t len) = 0;

	   /*!
		\brief
		ReadUntilEos read bytes from the interface until the EOS string
		was received or a timeout occurs.
		ReadUntilEos returns the count of bytes been readed. The received
		bytes are stored on the heap point by the readbuf pointer and
		must delete by the caller.
		\param readbuf points to the start of the readed bytes. You must
		delete them, also if you received no byte.
		\param readedBytes A pointer to the variable that receives the number
		of bytes read.
		\param eosString is the null terminated end of string sequence.
		Default is the linefeed character.
		\param timeout_in_ms the function returns after this time, also
		if no eos occured (default is 1s).
		\param quota defines a character between those an EOS doesn't
		terminate the string
		\return 1 on sucess (the operation ends successfull without a timeout),
		0 if a timeout occurred and -1 otherwise 
	   */
	   virtual int ReadUntilEOS(char*& readbuf,
						   size_t* readedBytes,
						   char* eosString = "\n",
						   long timeout_in_ms = 1000L,
						   char quota = 0);

	   /*!
		\brief
		readv() attempts to read up to len bytes from the interface
		into the buffer starting at buf.
		readv() is blocked till len bytes are readed or the given 
		timeout in milliseconds was reached.
		\param buf starting address of the buffer
		\param len count bytes, we want to read
		\param timeout_in_ms in milliseconds. If you don't want any timeout,
		you give the wxTIMEOUT_INFINITY here. 
		But think of it: In this case, this function never returns
		if there a not enough bytes to read.
		\return the number of data bytes successfully  read
	   */
	   int Readv(char* buf,size_t len,unsigned int timeout_in_ms);

	   /*!
		\brief
		readv() attempts to read up to len bytes from the interface
		into the buffer starting at buf.
		readv() is blocked till len bytes are readed or the timeout_flag
		points on a int greater then zero.
		\param buf starting adress of the buffer
		\param len count bytes, we want to read
		\param timeout_flag a pointer to an integer. If you don't want
		any timeout, you given a null pointer here.
		But think of it: In this case, this function comes never
		back, if there a not enough bytes to read.
		\param nice if true go to sleep for one ms (reduce CPU last),
		if there is no byte available (default is false)
	   */
	   int Readv(char* buf,size_t len,int* timeout_flag,bool nice=false);
    
	   /*!
		Write writes up to len bytes from the buffer starting with buf
		into the interface.
		\param buf start adress of the buffer
		\param len count of bytes, we want to write
		\return on success, the number of bytes written are returned 
		(zero indicates nothing was written).  On error, -1 is returned.
	   */
	   virtual int Write(char* buf,size_t len) = 0;

	   /*!
		Writev() writes up to len bytes to the interface from the buffer,
		starting at buf.
		Also Writev() blocks till all bytes are written or the given 
		timeout in milliseconds was reached.
		\param buf starting address of the buffer
		\param len count bytes, we want to write
		\param timeout_in_ms timeout in milliseconds. If you give 
		wxTIMEOUT_INFINITY here, the function blocks, till all data was
		written.
		\return the number of data bytes successfully written.
	   */
	   int Writev(char* buf,size_t len,unsigned int timeout_in_ms);

	   /*!
		Writev() writes up to len bytes to the interface from the buffer,
		starting at buf.
		Also Writev() blocks till all bytes are written or the timeout_flag
		points to an integer greater then zero.
		\param buf starting adress of the buffer
		\param len count bytes, we want to write
		\param timeout_flag a pointer to an integer. You also can give a null
		pointer here. This blocks, til all data is writen.
		\param nice if true go to sleep for one ms (reduce CPU last),
		if there is no byte available (default is false)
	   */
	   int Writev(char* buf,size_t len,int* timeout_flag,bool nice = false);

    };

} // namespace ctb

#endif



