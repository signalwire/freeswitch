/*
  Copyright (c) 2009, Adobe Systems Incorporated
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the 
    documentation and/or other materials provided with the distribution.
  
  * Neither the name of Adobe Systems Incorporated nor the names of its 
    contributors may be used to endorse or promote products derived from 
    this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.adobe.protocols.dict.util
{
	import com.adobe.net.proxies.RFC2817Socket;
	import flash.events.ProgressEvent;

	public class SocketHelper
		extends RFC2817Socket
	{
		private var terminator:String = "\r\n.\r\n";
		private var buffer:String;
		public static var COMPLETE_RESPONSE:String = "completeResponse";

		public function SocketHelper()
		{
			super();
			buffer = new String();
			addEventListener(ProgressEvent.SOCKET_DATA, incomingData);
		}

		private function incomingData(event:ProgressEvent):void
		{
			buffer += readUTFBytes(bytesAvailable);
			buffer = buffer.replace(/250[^\r\n]+\r\n/, ""); // Get rid of all 250s. Don't need them.
			var codeStr:String = buffer.substring(0, 3);
			if (!isNaN(parseInt(codeStr)))
			{
				var code:uint = uint(codeStr);
				if (code == 150 || code >= 200)
				{
					buffer = buffer.replace("\r\n", this.terminator);
				}
			}

			while (buffer.indexOf(this.terminator) != -1)
			{
				var chunk:String = buffer.substring(0, buffer.indexOf(this.terminator));
				buffer = buffer.substring(chunk.length + this.terminator.length, buffer.length);
				throwResponseEvent(chunk);
			}
		}
		
		private function throwResponseEvent(response:String):void
		{
			var responseEvent:CompleteResponseEvent = new CompleteResponseEvent(CompleteResponseEvent.COMPLETE_RESPONSE);
			responseEvent.response = response;
			dispatchEvent(responseEvent);			
		}
	}
}