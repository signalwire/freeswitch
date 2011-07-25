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

package com.adobe.air.logging
{
	import flash.filesystem.File;
	import flash.filesystem.FileMode;
	import flash.filesystem.FileStream;
	
	import mx.core.mx_internal;
	import mx.logging.targets.LineFormattedTarget;
	
	use namespace mx_internal;

	/**
	 * An Adobe AIR only class that provides a log target for the Flex logging
	 * framework, that logs files to a file on the user's system.
	 * 
	 * This class will only work when running within Adobe AIR>
	 */
	public class FileTarget extends LineFormattedTarget
	{
		private const DEFAULT_LOG_PATH:String = "app-storage:/application.log";
		
		private var log:File;
		
		public function FileTarget(logFile:File = null)
		{
			if(logFile != null)
			{
				log = logFile;
			}
			else
			{
				log = new File(DEFAULT_LOG_PATH);
			}
		}
		
		public function get logURI():String
		{
			return log.url;
		}
		
		mx_internal override function internalLog(message:String):void
	    {
			write(message);
	    }		
		
		private function write(msg:String):void
		{		
			var fs:FileStream = new FileStream();
				fs.open(log, FileMode.APPEND);
				fs.writeUTFBytes(msg + File.lineEnding);
				fs.close();
		}	
		
		public function clear():void
		{
			var fs:FileStream = new FileStream();
				fs.open(log, FileMode.WRITE);
				fs.writeUTFBytes("");
				fs.close();			
		}
		
	}
}