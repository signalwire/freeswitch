/*
  Copyright (c) 2008, Adobe Systems Incorporated
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

package com.adobe.net
{
	import flash.utils.ByteArray;
	
	/**
	 * This class implements an efficient lookup table for URI
	 * character escaping.  This class is only needed if you
	 * create a derived class of URI to handle custom URI
	 * syntax.  This class is used internally by URI.
	 * 
	 * @langversion ActionScript 3.0
	 * @playerversion Flash 9.0* 
	 */
	public class URIEncodingBitmap extends ByteArray
	{
		/**
		 * Constructor.  Creates an encoding bitmap using the given
		 * string of characters as the set of characters that need
		 * to be URI escaped.
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */
		public function URIEncodingBitmap(charsToEscape:String) : void
		{
			var i:int;
			var data:ByteArray = new ByteArray();
			
			// Initialize our 128 bits (16 bytes) to zero
			for (i = 0; i < 16; i++)
				this.writeByte(0);
				
			data.writeUTFBytes(charsToEscape);
			data.position = 0;
			
			while (data.bytesAvailable)
			{
				var c:int = data.readByte();
				
				if (c > 0x7f)
					continue;  // only escape low bytes
					
				var enc:int;
				this.position = (c >> 3);
				enc = this.readByte();
				enc |= 1 << (c & 0x7);
				this.position = (c >> 3);
				this.writeByte(enc);
			}
		}
		
		/**
		 * Based on the data table contained in this object, check
		 * if the given character should be escaped.
		 * 
		 * @param char	the character to be escaped.  Only the first
		 * character in the string is used.  Any other characters
		 * are ignored.
		 * 
		 * @return	the integer value of the raw UTF8 character.  For
		 * example, if '%' is given, the return value is 37 (0x25).
		 * If the character given does not need to be escaped, the
		 * return value is zero.
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0 
		 */
		public function ShouldEscape(char:String) : int
		{
			var data:ByteArray = new ByteArray();
			var c:int, mask:int;
			
			// write the character into a ByteArray so
			// we can pull it out as a raw byte value.
			data.writeUTFBytes(char);
			data.position = 0;
			c = data.readByte();
			
			if (c & 0x80)
			{
				// don't escape high byte characters.  It can make international
				// URI's unreadable.  We just want to escape characters that would
				// make URI syntax ambiguous.
				return 0;
			}
			else if ((c < 0x1f) || (c == 0x7f))
			{
				// control characters must be escaped.
				return c;
			}
			
			this.position = (c >> 3);
			mask = this.readByte();
			
			if (mask & (1 << (c & 0x7)))
			{
				// we need to escape this, return the numeric value
				// of the character
				return c;
			}
			else
			{
				return 0;
			}
		}
	}
}