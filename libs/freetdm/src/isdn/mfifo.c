/*****************************************************************************

  Filename:	 mfifo.c

  Description:  mfifo is a message orriented fifo system with support of
				both message and byte per byte retriaval of messages.

				The fifo has been designed with two usages in mind:

				-   Queueing of frames for hdlc and feeding out byte per byte
					with the possibility of re-sending of frames etc. 

				- fifo for messages of dynamic size.

				The fifo is allocated on top of any buffer and creates an
				index of message in the queue. The user can write/read
				messages or write messages and read the message one byte
				at the time.

  Interface:	
				MFIFOCreate		 Create/reset/initialize fifo.
				MFIFOClear		  Clear FIFO.
				MFIFOWriteMes	   Write message into fifo
				* MFIFOReadMes		Read message from fifo.
				MFIFOGetMesPtr	  Get ptr to next message.
				MFIFOKillNext	   Kill next message.

				* currently not implemented.

  Note:		 The message will always be saved continuously. If there is not
				sufficient space at the end of the buffer, the fifo will skip
				the last bytes and save the message at the top of the buffer.

				This is required to allow direct ptr access to messages 
				stored in the queue.

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are 
  met:

	* Redistributions of source code must retain the above copyright notice, 
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice, 
	  this list of conditions and the following disclaimer in the documentation 
	  and/or other materials provided with the distribution.
	* Neither the name of the Case Labs, Ltd nor the names of its contributors 
	  may be used to endorse or promote products derived from this software 
	  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
  POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "mfifo.h"
#include <memory.h>
#include <stdlib.h>

/*****************************************************************************

  Function:	 MFIFOCreate

  Description:  Creates a fifo on top of an existing buffer.

  Parameters:   buf	 ptr to buffer.
				size	size of buffer in bytes.
				index   size of index entries (max no messages).

  Return value: 0 if failure, 1 if ok.

*****************************************************************************/
int MFIFOCreate(unsigned char *buf, int size, int index)
{
	MFIFO *mf = (MFIFO *)buf;
	
	mf->first = mf->last = 0;
	mf->ixsize = index;
	mf->buf = &buf[sizeof(MFIFO) + (sizeof(MINDEX) * index)];

	if (mf->buf > &buf[size])
		return 0;

	mf->bsize = size - sizeof(MFIFO) - (sizeof(MINDEX) * index);

	return 1;
}

/*****************************************************************************

  Function:	 MFIFOClear

  Description:  Clear the FIFO

  Paremeters:   buf	 ptr to fifo

  Return Value: none

*****************************************************************************/
void MFIFOClear(unsigned char * buf)
{
	MFIFO *mf = (MFIFO *)buf;

	mf->first = mf->last = 0;
}

/*****************************************************************************

  Function:	 MFIFOGetLBOffset

  Description:  Helper function caclulating offset to the 'first out' byte.

  Paremeters:   buf	 ptr to fifo

  Return Value: offset.

*****************************************************************************/
int MFIFOGetLBOffset(unsigned char *buf)
{
	MFIFO *mf = (MFIFO *)buf;

	if (mf->last != mf->first)
		return mf->ix[mf->last].offset;
	
	return 0;
}

/*****************************************************************************

  Function:	 MFIFOGetFBOffset

  Description:  Helper function calculating the offset to the 'first in' 
				byte in the buffer. This is the position the next byte
				entering the fifo will occupy.

  Paremeters:   buf	 ptr to fifo

  Return Value: offset

*****************************************************************************/
int MFIFOGetFBOffset(unsigned char *buf)
{
	MFIFO *mf = (MFIFO *)buf;
	int x;

	if (mf->last == mf->first)
		return 0;

	x = mf->first - 1;

	if (x < 0)
		x = mf->ixsize - 1;

	return mf->ix[x].offset + mf->ix[x].size;
}

/*****************************************************************************

  Function:	 MFIFOWriteIX

  Description:  Helper function writing a calculated entry. The function
				will perform a memcpy to move the message and set the index
				values as well as increase the 'first in' index.

  Paremeters:   buf	 ptr to fifo
				mes	 ptr to message
				size	size of message in bytes.
				ix	  index to index entry.
				off	 offset to position to receive the message

  Return Value: none

*****************************************************************************/
void MFIFOWriteIX(unsigned char *buf, unsigned char *mes, int size, int ix, int off)
{
	MFIFO *mf = (MFIFO *)buf;
	int x;

	memcpy(&mf->buf[off], mes, size);
	mf->ix[ix].offset = off;
	mf->ix[ix].size = size;

	x = mf->first + 1;

	if (x >= mf->ixsize)
		x = 0;

	mf->first = x;
}

/*****************************************************************************

  Function:	 MFIFOWriteMes

  Description:  

  Paremeters:

  Return Value:

*****************************************************************************/
int MFIFOWriteMes(unsigned char *buf, unsigned char *mes, int size)
{
	MFIFO *mf = (MFIFO *)buf;
	int of, ol, x;

	x = mf->first + 1;

	if (x >= mf->ixsize)
		x = 0;

	if (x == mf->last)
		return 0; /* full queue */

	of = MFIFOGetFBOffset(buf);
	ol = MFIFOGetLBOffset(buf);
	if (mf->last == mf->first) { /* empty queue */
		mf->first = mf->last = 0; /* optimize */

		MFIFOWriteIX(buf, mes, size, mf->first, 0);
		return 1;
	}
	else if (of > ol) {
		if (mf->bsize - of >= size) {
			MFIFOWriteIX(buf, mes, size, mf->first, of);
			return 1;
		}
		else if (ol > size) {
			MFIFOWriteIX(buf, mes, size, mf->first, ol);
			return 1;
		}
	}
	else if (ol - of > size) {
			MFIFOWriteIX(buf, mes, size, mf->first, of);
			return 1;
	}

	return 0;
}

/*****************************************************************************

  Function:	 MFIFOGetMesPtr

  Description:  

  Paremeters:

  Return Value:

*****************************************************************************/
unsigned char * MFIFOGetMesPtr(unsigned char *buf, int *size)
{
	MFIFO *mf = (MFIFO *)buf;

	if (mf->first == mf->last) {
		return NULL;
	}

	*size = mf->ix[mf->last].size;
	return &mf->buf[mf->ix[mf->last].offset];
}

/*****************************************************************************

  Function:	 MFIFOKillNext

  Description:  

  Paremeters:

  Return Value:

*****************************************************************************/
void MFIFOKillNext(unsigned char *buf)
{
	MFIFO *mf = (MFIFO *)buf;
	int x;

	if (mf->first != mf->last) {
		x = mf->last + 1;
		if (x >= mf->ixsize) {
			x = 0;
		}

		mf->last = x;
	}
}


/*
 * Queue-style accessor functions
 */

/**
 * MFIFOGetMesPtrOffset
 * \brief	Get pointer to and size of message at position x
 */
unsigned char * MFIFOGetMesPtrOffset(unsigned char *buf, int *size, const int pos)
{
	MFIFO *mf = (MFIFO *)buf;
	int x;

	if (mf->first == mf->last) {
		return NULL;
	}

	if (pos < 0 || pos >= mf->ixsize) {
		return NULL;
	}

	x = pos - mf->last;
	if (x < 0) {
		x += (mf->ixsize - 1);
	}

	*size = mf->ix[x].size;
	return &mf->buf[mf->ix[x].offset];
}


/**
 * MFIFOGetMesCount
 * \brief	How many messages are currently in the buffer?
 */
int MFIFOGetMesCount(unsigned char *buf)
{
	MFIFO *mf = (MFIFO *)buf;

	if (mf->first == mf->last) {
		return 0;
	}
	else if (mf->first > mf->last) {
		return mf->first - mf->last;
	}
	else {
		return (mf->ixsize - mf->last) + mf->first;
	}
}

/**
 * MFIFOWriteMesOverwrite
 * \brief	Same as MFIFOWriteMes but old frames will be overwritten if the fifo is full
 */
int MFIFOWriteMesOverwrite(unsigned char *buf, unsigned char *mes, int size)
{
	MFIFO *mf = (MFIFO *)buf;
	int of, ol, x;

	x = mf->first + 1;

	if (x >= mf->ixsize)
		x = 0;

	if (x == mf->last) {
		/* advance last pointer */
		mf->last++;

		if (mf->last >= mf->ixsize)
			mf->last = 0;
	}

	of = MFIFOGetFBOffset(buf);
	ol = MFIFOGetLBOffset(buf);

	if (mf->last == mf->first) {	/* empty queue */
		mf->first = mf->last = 0;	/* optimize */

		MFIFOWriteIX(buf, mes, size, mf->first, 0);
		return 1;
	}
	else if (of > ol) {
		if (mf->bsize - of >= size) {
			MFIFOWriteIX(buf, mes, size, mf->first, of);
			return 1;
		}
		else if (ol > size) {
			MFIFOWriteIX(buf, mes, size, mf->first, ol);
			return 1;
		}
	}
	else if (ol - of > size) {
		MFIFOWriteIX(buf, mes, size, mf->first, of);
		return 1;
	}
	return 0;
}
