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


package com.adobe.webapis.events
{

	import flash.events.Event;

	/**
	* Event class that contains data loaded from remote services.
	*
	* @author Mike Chambers
	*/
	public class ServiceEvent extends Event
	{
		private var _data:Object = new Object();;

		/**
		* Constructor for ServiceEvent class.
		*
		* @param type The type of event that the instance represents.
		*/
		public function ServiceEvent(type:String, bubbles:Boolean = false, 
														cancelable:Boolean=false)
		{
			super(type, bubbles, cancelable);
		}

		/**
		* 	This object contains data loaded in response
		* 	to remote service calls, and properties associated with that call.
		*/
		public function get data():Object
		{
			return _data;
		}

		public function set data(d:Object):void
		{
			_data = d;
		}
		
		public override function clone():Event
		{
			var out:ServiceEvent = new ServiceEvent(type, bubbles, cancelable);
			out.data = data;
			
			return out;
		}

	}
}