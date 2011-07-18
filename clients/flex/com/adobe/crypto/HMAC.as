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

package com.adobe.crypto {
	import flash.utils.ByteArray;
	import flash.utils.Endian;
	import flash.utils.describeType;
	/**
	 * Keyed-Hashing for Message Authentication
	 * Implementation based on algorithm description at 
	 * http://www.faqs.org/rfcs/rfc2104.html
	 */
	public class HMAC 
	{
		/**
		 * Performs the HMAC hash algorithm using byte arrays.
		 *
		 * @param secret The secret key
		 * @param message The message to hash
		 * @param algorithm Hash object to use
		 * @return A string containing the hash value of message
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 8.5
		 * @tiptext
		 */
		public static function hash( secret:String, message:String, algorithm:Object = null ):String
		{
			var text:ByteArray = new ByteArray();
			var k_secret:ByteArray = new ByteArray();
			
			text.writeUTFBytes(message);
			k_secret.writeUTFBytes(secret);
			
			return hashBytes(k_secret, text, algorithm);
		}
		
		/**
		 * Performs the HMAC hash algorithm using string.
		 *
		 * @param secret The secret key
		 * @param message The message to hash
		 * @param algorithm Hash object to use
		 * @return A string containing the hash value of message
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 8.5
		 * @tiptext
		 */
		public static function hashBytes( secret:ByteArray, message:ByteArray, algorithm:Object = null ):String
		{
			var ipad:ByteArray = new ByteArray();
			var opad:ByteArray = new ByteArray();
			var endian:String = Endian.BIG_ENDIAN;
			
			if(algorithm == null){
				algorithm = MD5;
			}
			
			if ( describeType(algorithm).@name.toString() == "com.adobe.crypto::MD5" ) {
				endian = Endian.LITTLE_ENDIAN;
			}
			
			if ( secret.length > 64 ) {
				algorithm.hashBytes(secret);
				secret = new ByteArray();
				secret.endian = endian;
				
				while ( algorithm.digest.bytesAvailable != 0 ) {
					secret.writeInt(algorithm.digest.readInt());
				}
			}

			secret.length = 64
			secret.position = 0;
			for ( var x:int = 0; x < 64; x++ ) {
				var byte:int = secret.readByte();
				ipad.writeByte(0x36 ^ byte);
				opad.writeByte(0x5c ^ byte);
			}
			
			ipad.writeBytes(message);
			algorithm.hashBytes(ipad);
			var tmp:ByteArray = new ByteArray();
			tmp.endian = endian;	
			
			while ( algorithm.digest.bytesAvailable != 0 ) {
				tmp.writeInt(algorithm.digest.readInt());
			}
			tmp.position = 0;
			
			while ( tmp.bytesAvailable != 0 ) {
				opad.writeByte(tmp.readUnsignedByte());
			}
			return algorithm.hashBytes( opad );
		}
		
	}
	
}
