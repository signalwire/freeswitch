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
	import flash.events.EventDispatcher;
	import flash.utils.Timer;
	import flash.events.TimerEvent;
	import flash.filesystem.File;
	import flash.utils.Dictionary;
	import com.adobe.air.filesystem.events.FileMonitorEvent;
	import com.adobe.utils.ArrayUtil;

	/**
	* Dispatched when a volume is added to the system.
	*
	* @eventType com.adobe.air.filesystem.events.FileMonitor.ADD_VOLUME
	*/
	[Event(name="ADD_VOLUME", type="com.adobe.air.filesystem.events.FileMonitor")]	

	/**
	* Dispatched when a volume is removed from the system.
	*
	* @eventType com.adobe.air.filesystem.events.FileMonitor.REMOVE_VOLUME
	*/
	[Event(name="REMOVE_VOLUME", type="com.adobe.air.filesystem.events.FileMonitor")]	

	/**
	 * Class that monitors changes to the File volumes attached to the operating
	 * system.
	 */ 
	public class VolumeMonitor extends EventDispatcher
	{
		private var timer:Timer;
		private var _interval:Number;
		private static const DEFAULT_MONITOR_INTERVAL:Number = 2000;
		
		private var volumes:Dictionary;
		
		/**
		 * 	Constructor.
		 * 
		 * 	@param interval How often in milliseconds the system is polled for
		 * 	volume change events. Default value is 2000, minimum value is 1000
		 */
		public function VolumeMonitor(interval:Number = -1)
		{
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
		 * 	How often the system is polled for Volume change events.
		 */
		public function get interval():Number
		{
			return _interval;
		}
		
		/**
		 * Begins the monitoring of changes to the attached File volumes.
		 */
		public function watch():void
		{
			if(!timer)
			{
				timer = new Timer(_interval);
				timer.addEventListener(TimerEvent.TIMER, onTimerEvent,false,0, true);
			}
			
			//we reinitialize the hash everytime we start watching
			volumes = new Dictionary();
			
			var v:Array = FileUtil.getRootDirectories();
			for each(var f:File in v)
			{
				//null or undefined
				if(volumes[f.url] == null)
				{
					volumes[f.url] = f;
				}
			}			
			
			timer.start();
		}
		
		/**
		 * Stops monitoring for changes to the attached File volumes.
		 */
		public function unwatch():void
		{
			timer.stop();
			timer.removeEventListener(TimerEvent.TIMER, onTimerEvent);
		}
		
		private function onTimerEvent(e:TimerEvent):void
		{
			var v:Array = FileUtil.getRootDirectories();
			
			var outEvent:FileMonitorEvent;
			var found:Boolean = false;
			for(var key:String in volumes)
			{
				for each(var f:File in v)
				{
					//trace("--\n" + key);
					//trace(f.url);
					if(f.url == key)
					{
						found = true;
						break;
					}
				}
				
				if(!found)
				{
					outEvent = new FileMonitorEvent(FileMonitorEvent.REMOVE_VOLUME);
					outEvent.file = volumes[key];
					dispatchEvent(outEvent);
					delete volumes[key];
				}
				
				found  = false;
			}
			
			for each(var f2:File in v)
			{
				//null or undefined
				if(volumes[f2.url] == null)
				{
					volumes[f2.url] = f2;
					outEvent = new FileMonitorEvent(FileMonitorEvent.ADD_VOLUME);
					outEvent.file = f2;
					
					dispatchEvent(outEvent);
				}
			}
		}
		

	}
}