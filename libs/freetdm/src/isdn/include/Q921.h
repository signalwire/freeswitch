/*****************************************************************************

  FileName:     q921.h

  Description:  Contains headers of a Q.921 protocol on top of the Comet 
                Driver.

                Most of the work required to execute a Q.921 protocol is 
                taken care of by the Comet ship and it's driver. This layer
                will simply configure and make use of these features to 
                complete a Q.921 implementation.

  Note:         This header file is the only include file that should be 
                acessed by users of the Q.921 stack.

  Interface:    The Q.921 stack contains 2 layers. 

                -   One interface layer.
                -   One driver layer.

                The interface layer contains the interface functions required 
                for a layer 3 stack to be able to send and receive messages.

                The driver layer will simply feed bytes into the ship as
                required and queue messages received out from the ship.

                Q921TimeTick        The Q.921 like any other blackbox 
                                    modules contains no thread by it's own
                                    and must therefore be called regularly 
                                    by an external 'thread' to do maintenance
                                    etc.

                Q921Rx32            Receive message from layer 3. Called by
                                    the layer 3 stack to send a message.

                Q921Tx23            Send a message to layer 3. 

                OnQ921Error         Function called every if an error is 
                                    deteceted.

                OnQ921Log           Function called if logging is active.


                <TODO> Maintenance/Configuration interface

  Created:      27.dec.2000/JVB

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

#ifndef _Q921
#define _Q921

#define Q921MAXTRUNK 4
#define Q921MAXHDLCSPACE 3000

typedef struct
{
    char HDLCInQueue[Q921MAXHDLCSPACE];
    unsigned char vs;
    unsigned char vr;
    int state;
}Q921Data;

void Q921Init();
void Q921SetHeaderSpace(int hspace);
void Q921SetTx21CB(int (*callback)(int dev, unsigned char *, int));
void Q921SetTx23CB(int (*callback)(int dev, unsigned char *, int));
int Q921QueueHDLCFrame(int trunk, char *b, int size);
int Q921Rx12(long trunk);

#endif

