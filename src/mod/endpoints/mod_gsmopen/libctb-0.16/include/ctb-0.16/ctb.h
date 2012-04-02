#ifndef LIBCTB_CTB_H_INCLUDED_
#define LIBCTB_CTB_H_INCLUDED_

#include "ctb-0.16/fifo.h"
#if ( GPIB )
# include "ctb-0.16/gpib.h"
#endif
#include "ctb-0.16/getopt.h"
#include "ctb-0.16/iobase.h"
//#include "ctb-0.16/kbhit.h"
#include "ctb-0.16/portscan.h"
#include "ctb-0.16/serport.h"
#include "ctb-0.16/serportx.h"
#include "ctb-0.16/timer.h"

/*!
  \mainpage ctb overview

  The ctb (communication toolbox) library was realized, to simplify the
  communication with other instruments throughout the serial com ports
  (at first). To make my life easier, it should works with Linux and
  all win32 plattforms (excepted windows 3.1, which is a only 16bit OS)
  because I develope my applications for both plattforms).\n
  Some times later GPIB support was added to make ctb an integrated part
  for the extensive test and calibration system of a company I worked
  these days.\n
  The main goal of the library was a non-blocked communication to avoid
  frozen GUIs waiting for data which in some conditions never arrives.
  
  On the base ctb defines an abstract class IOBase, which must be
  derivate for several interfaces (at now this was done for the
  RS232 comports and GPIB IEEE488 interface).

  This leads to another feature: Because all classes depends on one
  super class, you have just open your wanted interface and don't
  worry about it's special typ later. This is like the 'Virtual
  Instrument' featured by Nation Instruments LabView.

  Last not least: ctb provides one written code for Linux and Windows
  (compiles well with GNU G++ and VC++). Without any dependences (execept
  for a standard C++ compilier) ctb runs also in small enviroments like
  embedded systems and doesn't need any graphic stuff for use.

    ctb is composed of five parts:

  - ctb::IOBase class
  - ctb::SerialPort class
  - ctb::GpibDevice class
  - ctb::Timer class
  - ctb::Fifo class

  \section IOBase IOBase class

  An abstract class for different interfaces. The idea behind this:
  Similar to the virtual file system this class defines a lot of
  preset member functions, which the derivate classes must be
  overload.\n
  In the main thing these are: open a interface (such as RS232),
  reading and writing non blocked through the interface and at
  last, close it.\n
  For special interface settings the method ioctl was defined.
  (control interface). ioctl covers some interface dependent
  settings like switch on/off the RS232 status lines and must also
  be defined from each derivated class.


  \section SerialPort SerialPort class

  The class for the serial ports is named as
  ctb::SerialPort. SerialPort is a wrapper for non blocked reading and
  writing. This is easy under linux, but with windows a lot more
  tricky. SerialPort is as simple as possible. It doesn't create any
  gui events or signals, so it works also standalone. It's also not a
  device driver, means, you must call the read method, if you look for
  receiving data.

  You can write any desired data with any length (length type is
  size_t, I think, on win32 and linux this is a 32Bit integer) and
  SerialPort returns the really writen data length, also you can read
  a lot of data and SerialPort returns the really received data count.

  Both, read and write returns immediatelly. Using these, the program
  never blocks.  Also IOBase implements a blocked read and write. You can
  use these functions, if you want a definitiv count of data and never
  accept less than this. Because there is a difficulty, when the
  communication is interrupted or death, both blocked functions get a
  timeout flag to returns after a given time interval.  The timeouts
  will be handled with the second timer class.

  As an additional benefit ctb features also 9 Bit transmission (with
  take advantage of the parity bit), non-standard baudrates (depending
  on your hardware but not on ctb) and all parity eventualities
  including static parity settings like Mark and Space.

  \section GpibDevice GpibDevice class
  
  Named as ctb::GpibDevice. In the philosophy of the SerialPort class
  GpibDevice also supports non-blocking communication. You can instant
  as many GpibDevice objects as you need for instance to communicate 
  with a lot of different bus participants in a typical GPIB enviroment.
  GPIB support was tested with PCI cards and USB adapter from Nation
  Instrument and Keithley.

  \section Timer Timer class

  The idea of the ctb::Timer class is to base on the Unix C alarm
  function. You create a Timer with a given alarm time and a adress of
  flag, which the timer must set after the time is over.

  Because the alarm function cannot used more than once in the same
  process (under windows I don't know a similar function), every timer
  instance will be a separate thread after starting it. So you can start
  a timer and continue in your program, make a lot of things and test
  the flag whenever you want this. (For example, you read/write a
  given count of data).

  \note
  I think, it's a better style, to request a given count of data in
  100ms (for example) and trap the situation, if there are not enough
  data after this time. And not do this for every byte!

  \section Fifo Fifo cass
  Provides a simple thread safe fifo to realize a fast and simple
  communication pipe between two threads (and was used also as a put
  back mechanism for the wxIOBase and it's derivated classes).\n
  ctb::Fifo tackles the concurrently access from different threads with an
  internal temporary pointer asignment which was atomic. From there no
  mutex or semaphore is involved and lead to a fast access.

  Please note:\n
  The thread safeness is limited to the put/get write/read methods but
  which should be sufficent for a fifo.


  \latexonly \newpage \endlatexonly

 */

#endif
