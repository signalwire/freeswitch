#ifndef __LIBCTB_FIFO_H_INCLUDED_
#define __LIBCTB_FIFO_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        fifo.h
// Purpose:
// Author:      Joachim Buermann, Michael Hungershausen
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>

/**
   \file fifo.h
*/

namespace ctb {

/*!
  \class Fifo
  A simple thread safe fifo to realize a put back mechanism for the 
  wxIOBase and it's derivated classes.
*/
    class Fifo
    {
    protected:
	   /*! the size of the fifo */
	   size_t m_size;
	   /*! the start of the internal fifo buffer */
	   char* m_begin;
	   /*! 
		the end of the internal fifo buffer (m_end marks the first
		invalid byte AFTER the internal buffer)
	   */
	   char* m_end;
	   /*! the current read position */
	   char* m_rdptr;
	   /*! the current write position */
	   char* m_wrptr;
    public:
	   /*!
		\brief the constructor initialize a fifo with the given size.
		\param size size of the fifo
	   */
	   Fifo(size_t size);
	   /*!
		\brief the destructor destroys all internal memory.
	   */
	   virtual ~Fifo();
	   /*!
		\brief clear all internal memory and set the read and write
		pointers to the start of the internal memory.
		\Note This function is not thread safe! Don't use it, if another 
		thread takes access to the fifo instance. Use a looping get() or
		read() call instead of this.
	   */
	   virtual void clear();
	   /*!
		\brief fetch the next available byte from the fifo.
		\param ch points to a charater to store the result
		\return 1 if successful, 0 otherwise
	   */
	   virtual int get(char* ch);
	   /*!
		\brief query the fifo for it's available bytes.
		\return count of readable bytes, storing in the fifo 
	   */
	   size_t items();
	   /*!
		\brief put a character into the fifo.
		\param ch the character to put in
		\return 1 if successful, 0 otherwise
	   */
	   virtual int put(char ch);
	   /*!
		\brief read a given count of bytes out of the fifo.
		\param data memory to store the readed data
		\param count number of bytes to read
		\return On success, the number of bytes read are returned, 
		0 otherwise
	   */
	   virtual int read(char* data,int count);
	   /*!
		\brief write a given count of bytes into the fifo.
		\param data start of the data to write
		\param count number of bytes to write
		\return On success, the number of bytes written are returned, 
		0 otherwise
	   */
	   virtual int write(char* data,int count);
    };

} // namespace ctb

#endif
