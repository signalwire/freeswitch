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

package com.adobe.air.filesystem
{
	import flash.filesystem.File;
	import flash.utils.Timer;
	import flash.events.TimerEvent;
	import flash.events.Event;
	import flash.events.EventDispatcher;
	import com.adobe.air.filesystem.events.FileMonitorEvent;

	/*
		Todo:
		
		-Cmonitor changes in multiple attributes
		-add support for monitoring multiple files
	*/
	
	/**
	* Dispatched when the modified date of the file being modified changes.
	*
	* @eventType com.adobe.air.filesystem.events.FileMonitor.CHANGE
	*/
	[Event(name="CHANGE", type="com.adobe.air.filesystem.events.FileMonitor")]		

	/**
	* Dispatched when the file being monitored is moved or deleted. The file 
	* will be unwatched.
	*
	* @eventType com.adobe.air.filesystem.events.FileMonitor.MOVE
	*/
	[Event(name="MOVE", type="com.adobe.air.filesystem.events.FileMonitor")]	
	
	/**
	* Dispatched when the file being monitored is created.
	*
	* @eventType com.adobe.air.filesystem.events.FileMonitor.CREATE
	*/
	[Event(name="CREATE", type="com.adobe.air.filesystem.events.FileMonitor")]			
	
	/**
	* Class that monitors files for changes.
	*/
	public class FileMonitor extends EventDispatcher
	{
		private var _file:File;
		private var timer:Timer;
		public static const DEFAULT_MONITOR_INTERVAL:Number = 1000;
		private var _interval:Number;
		private var fileExists:Boolean = false;
		
		private var lastModifiedTime:Number;
		
		/**
		 *  Constructor
		 * 
		 * 	@parameter file The File that will be monitored for changes.
		 * 
		 * 	@param interval How often in milliseconds the file is polled for
		 * 	change events. Default value is 1000, minimum value is 1000
		 */ 
		public function FileMonitor(file:File = null, interval:Number = -1)
		{
			this.file = file;
			
			if(interval != -1)
			{
				if(interval < 1000)
				{
					_interval = 1000;
				}
				else
				{
					_interval = interval;
				}
			}
			else
			{
				_interval = DEFAULT_MONITOR_INTERVAL;
			}
		}
	
		/**
		 * File being monitored for changes.
		 * 
		 * Setting the property will result in unwatch() being called.
		 */
		public function get file():File
		{
			return _file;
		}
		
		public function set file(file:File):void
		{			
			if(timer && timer.running)
			{
				unwatch();
			}
			
			_file = file;
			
			if(!_file)
			{
				fileExists = false;
				return;
			}

			//note : this will throw an error if new File() is passed in.
			fileExists = _file.exists;
			if(fileExists)
			{
				lastModifiedTime = _file.modificationDate.getTime();
			}	

		}
		
		/**
		 * 	How often the system is polled for Volume change events.
		 */
		public function get interval():Number
		{
			return _interval;
		}		
		
		/**
		 * Begins monitoring the specified file for changes.
		 * 
		 * Broadcasts Event.CHANGE event when the file's modification date has changed.
		 */
		public function watch():void
		{
			if(!file)
			{
				//should we throw an error?
				return;
			}
			
			if(timer && timer.running)
			{
				return;
			}
			
			//check and see if timer is active. if it is, return
			if(!timer)
			{
				timer = new Timer(_interval);
				timer.addEventListener(TimerEvent.TIMER, onTimerEvent, false, 0, true);
			}
			
			timer.start();
		}
		
		/**
		 * Stops watching the specified file for changes.
		 */
		public function unwatch():void
		{
			if(!timer)
			{
				return;
			}
			
			timer.stop();
			timer.removeEventListener(TimerEvent.TIMER, onTimerEvent);
		}
		
		private function onTimerEvent(e:TimerEvent):void
		{
			var outEvent:FileMonitorEvent;
			
			if(fileExists != _file.exists)
			{
				if(_file.exists)
				{
					//file was created
					outEvent = new FileMonitorEvent(FileMonitorEvent.CREATE);
					lastModifiedTime = _file.modificationDate.getTime();
				}
				else
				{
					//file was moved / deleted
					outEvent = new FileMonitorEvent(FileMonitorEvent.MOVE);
					unwatch();
				}
				fileExists = _file.exists;
			}
			else
			{	
				if(!_file.exists)
				{
					return;
				}				
					
				var modifiedTime:Number = _file.modificationDate.getTime();
				
				if(modifiedTime == lastModifiedTime)
				{
					return;
				}
				
				lastModifiedTime = modifiedTime;
				
				//file modified
				outEvent = new FileMonitorEvent(FileMonitorEvent.CHANGE);
			}
			
			if(outEvent)
			{
				outEvent.file = _file;
				dispatchEvent(outEvent);
			}

		}
	}
}
