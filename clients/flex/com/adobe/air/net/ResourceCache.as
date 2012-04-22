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

package com.adobe.air.net
{
	import com.adobe.crypto.MD5;
	import com.adobe.net.DynamicURLLoader;
	
	import flash.events.Event;
	import flash.events.EventDispatcher;
	import flash.events.IOErrorEvent;
	import flash.filesystem.File;
	import flash.filesystem.FileMode;
	import flash.filesystem.FileStream;
	import flash.net.URLLoaderDataFormat;
	import flash.net.URLRequest;
	import flash.utils.ByteArray;
	import com.adobe.air.net.events.ResourceCacheEvent;

	//todo: add event metadata

	public class ResourceCache extends EventDispatcher
	{
		private var _cacheName:String;
		//maybe rename to make it clearer it loads data
		public function ResourceCache(cacheName:String)
		{
			_cacheName = cacheName;
		}
		
		public function get cacheName():String
		{
			return _cacheName;
		}
		
		private function getStorageDir():File
		{
			return File.applicationStorageDirectory.resolvePath(_cacheName);
		}
		
		public function itemExists(key:String):Boolean
		{
			return getItemFile(key).exists;
		}
		
		public function clearCache():void
		{
			var cacheDir:File = getStorageDir();
			try
			{
				cacheDir.deleteDirectory(true);
			}
			catch (e:IOErrorEvent)
			{
				// we tried!
			}
		}
		
		public function getItemFile(key:String):File
		{
			var dir:File = getStorageDir();
			var fName:String = generateKeyHash(key);
			var file:File = dir.resolvePath(fName);
			
			return file;
		}
		
		public function retrieve(url:String):void
		{
			
			var key:String = generateKeyHash(url);
			var file:File = getItemFile(key);
			
			//todo: do we need to check if the dir exists?
			
			if(file.exists)
			{
				var e:ResourceCacheEvent = new ResourceCacheEvent(ResourceCacheEvent.ITEM_READY);
					e.key = key;
					e.file = file;
					
				dispatchEvent(e);	
				return;
			}
			
			
			var loader:DynamicURLLoader = new DynamicURLLoader();
				loader.file = file;
				loader.key = key;
				
				loader.addEventListener(Event.COMPLETE, onDataLoad);
				loader.addEventListener(IOErrorEvent.IO_ERROR, onLoadError);
				
				loader.dataFormat = URLLoaderDataFormat.BINARY;
				
				loader.load(new URLRequest(url));
			
		}
		
		private function onLoadError(event:IOErrorEvent):void
		{
			trace("onLoadError : could not cache item");
		}
		
		private function onDataLoad(event:Event):void
		{
			var loader:DynamicURLLoader = DynamicURLLoader(event.target);
			
			var f:File = File(loader.file);
			var key:String = String(loader.key);
			
			var fileStream:FileStream = new FileStream();
				fileStream.open(f, FileMode.WRITE);
				fileStream.writeBytes(loader.data as ByteArray);
				fileStream.close();
				
				var g:ResourceCacheEvent = new ResourceCacheEvent(ResourceCacheEvent.ITEM_CACHED);
					g.key = key;
					g.file = f;
					
				dispatchEvent(g);	
				
				var e:ResourceCacheEvent = new ResourceCacheEvent(ResourceCacheEvent.ITEM_READY);
					e.key = key;
					e.file = f;
					
				dispatchEvent(e);	
		}
		
		
		private function generateKeyHash(key:String):String
		{
			return MD5.hash(key);
		}
	}
}
