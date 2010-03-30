/*****************************************************************************

  Filename:     mfifo.h

  Contents:     header for MFIFO

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
#ifndef _MFIFO
#define _MFIFO

/*****************************************************************************

  Struct:		MINDEX

  Description:	Message Index used to index a dynamic size Message FIFO.

*****************************************************************************/
typedef struct _mindex {
    int offset;                     /* offset to message in buf             */
    int size;                       /* size of message in bytes             */
} MINDEX;

/*****************************************************************************

  Struct:		MFIFO

  Description:	Message FIFO. Provides a dynamic sized message based FIFO
				queue.

*****************************************************************************/
typedef struct {
	int first;                      /* first out                            */
	int last;                       /* last in + 1                          */
	int bsize;                      /* buffer size                          */
	unsigned char *buf;             /* ptr to start of buffer               */
	int ixsize;                     /* index size                           */
	MINDEX ix[1];                   /* message index                        */
} MFIFO;

/*****************************************************************************
  Function prototypes.
*****************************************************************************/
int MFIFOCreate(unsigned char *buf, int size, int index);
void MFIFOClear(unsigned char * buf);
int MFIFOGetLBOffset(unsigned char *buf);
int MFIFOGetFBOffset(unsigned char *buf);
void MFIFOWriteIX(unsigned char *buf, unsigned char *mes, int size, int ix, int off);
int MFIFOWriteMes(unsigned char *buf, unsigned char *mes, int size);
unsigned char * MFIFOGetMesPtr(unsigned char *buf, int *size);
void MFIFOKillNext(unsigned char *buf);

unsigned char * MFIFOGetMesPtrOffset(unsigned char *buf, int *size, const int pos);
int MFIFOGetMesCount(unsigned char *buf);
int MFIFOWriteMesOverwrite(unsigned char *buf, unsigned char *mes, int size);

#endif
