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

package com.adobe.crypto
{
	import mx.formatters.DateFormatter;
	import mx.utils.Base64Encoder;
	
	/**
	 * Web Services Security Username Token
	 *
	 * Implementation based on algorithm description at 
	 * http://www.oasis-open.org/committees/wss/documents/WSS-Username-02-0223-merged.pdf
	 */
	public class WSSEUsernameToken
	{
		/**
		 * Generates a WSSE Username Token.
		 *
		 * @param username The username
		 * @param password The password
		 * @param nonce A cryptographically random nonce (if null, the nonce
		 * will be generated)
		 * @param timestamp The time at which the token is generated (if null,
		 * the time will be set to the moment of execution)
		 * @return The generated token
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 * @tiptext
		 */
		public static function getUsernameToken(username:String, password:String, nonce:String=null, timestamp:Date=null):String
		{
			if (nonce == null)
			{
				nonce = generateNonce();
			}
			nonce = base64Encode(nonce);
		
			var created:String = generateTimestamp(timestamp);
		
			var password64:String = getBase64Digest(nonce,
				created,
				password);
		
			var token:String = new String("UsernameToken Username=\"");
			token += username + "\", " +
					 "PasswordDigest=\"" + password64 + "\", " +
					 "Nonce=\"" + nonce + "\", " +
					 "Created=\"" + created + "\"";
			return token;
		}
		
		private static function generateNonce():String
		{
			// Math.random returns a Number between 0 and 1.  We don't want our
			// nonce to contain invalid characters (e.g. the period) so we
			// strip them out before returning the result.
			var s:String =  Math.random().toString();
			return s.replace(".", "");
		}
		
		internal static function base64Encode(s:String):String
		{
			var encoder:Base64Encoder = new Base64Encoder();
			encoder.encode(s);
			return encoder.flush();
		}
		
		internal static function generateTimestamp(timestamp:Date):String
		{
			if (timestamp == null)
			{
				timestamp = new Date();
			}
			var dateFormatter:DateFormatter = new DateFormatter();
			dateFormatter.formatString = "YYYY-MM-DDTJJ:NN:SS"
			return dateFormatter.format(timestamp) + "Z";
		}
		
		internal static function getBase64Digest(nonce:String, created:String, password:String):String
		{
			return SHA1.hashToBase64(nonce + created + password);
		}
	}
}