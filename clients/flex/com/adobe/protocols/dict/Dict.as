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

package com.adobe.protocols.dict
{
	import com.adobe.protocols.dict.events.*;
	import com.adobe.protocols.dict.util.*;
	
	import flash.events.Event;
	import flash.events.EventDispatcher;
	import flash.events.IOErrorEvent;
	import flash.events.ProgressEvent;
	import flash.events.SecurityErrorEvent;
	import flash.net.Socket;
	import mx.rpc.http.HTTPService;
	import mx.rpc.events.ResultEvent;
	import mx.rpc.events.FaultEvent;
	import flash.xml.XMLNode;
	import mx.utils.StringUtil;

	public class Dict
		extends EventDispatcher
	{
		// Event type names.
		//public static var CONNECTED:String = "connected";
		//public static var DISCONNECTED:String = "disconnected";
		public static var IO_ERROR:String = IOErrorEvent.IO_ERROR;
		//public static var ERROR:String = "error";
		//public static var SERVERS:String = "servers";
		//public static var DATABASES:String = "databases";
		//public static var MATCH_STRATEGIES:String = "matchStrategies";
		//public static var DEFINITION:String = "definition";
		//public static var DEFINITION_HEADER:String = "definitionHeader";
		//public static var MATCH:String = "match";
		//public static var NO_MATCH:String = "noMatch";

		public static var FIRST_MATCH:uint = 0;
		public static var ALL_DATABASES:uint = 1;

		private var socket:SocketHelper;
		
		private var dbShortList:Boolean;

		public function Dict()
		{
			this.socket = new SocketHelper();
			this.socket.addEventListener(Event.CONNECT, connected);
			this.socket.addEventListener(Event.CLOSE, disconnected);
			this.socket.addEventListener(SocketHelper.COMPLETE_RESPONSE, incomingData);
			this.socket.addEventListener(IOErrorEvent.IO_ERROR, ioError);
			this.socket.addEventListener(SecurityErrorEvent.SECURITY_ERROR, securityError);
		}

		public function connect(server:String, port:uint = 2628):void
		{
			if (this.socket.connected)
			{
				this.socket.close();
			}
			this.socket.connect(server, port);
		}

		public function connectThroughProxy(proxyServer:String,
											proxyPort:int,
											server:String,
											port:uint = 2628):void
		{
			if (this.socket.connected)
			{
				this.socket.close();
			}
			this.socket.setProxyInfo(proxyServer, proxyPort);
			this.socket.connect(server, port);
		}

		public function disconnect():void
		{
			this.socket.close();
			this.disconnected(null);
		}

		public function getServers():void
		{
			var http:HTTPService = new HTTPService();
			http.url = "http://luetzschena-stahmeln.de/dictd/xmllist.php";
			http.addEventListener(ResultEvent.RESULT, incomingServerXML);
			http.addEventListener(FaultEvent.FAULT, httpError);
			http.resultFormat = HTTPService.RESULT_FORMAT_E4X;
			http.send();
		}

		public function getDatabases(shortList:Boolean=true):void
		{
			this.dbShortList = shortList;
			this.socket.writeUTFBytes("show db\r\n");
			this.socket.flush();
		}

		public function getMatchStrategies():void
		{
			this.socket.writeUTFBytes("show strat\r\n");
			this.socket.flush();
		}

		public function match(database:String, term:String, scope:String="prefix"):void
		{
			this.socket.writeUTFBytes("match " + database + " " + scope + " \"" + term + "\"\r\n");
			this.socket.flush();
		}

		public function define(database:String, term:String):void
		{
			this.socket.writeUTFBytes("define " + database + " \"" + term + "\"\r\n");
			this.socket.flush();
		}

		public function lookup(term:String, scope:uint):void
		{
			var flag:String;
			if (scope == Dict.ALL_DATABASES)
			{
				flag = "*";
			}
			else if (scope == Dict.FIRST_MATCH)
			{
				flag = "!";
			}
			this.socket.writeUTFBytes("define " + flag + " \"" + term + "\"\r\n");
			this.socket.flush();
		}

		//// Private functions ////

		private function connected(event:Event):void
		{
        	// Wait to dispatch an event until we get the 220 response.
    	}

		private function disconnected(event:Event):void
		{
        	dispatchEvent(new DisconnectedEvent(DisconnectedEvent.DISCONNECTED));
    	}

		private function incomingServerXML(event:ResultEvent):void
		{
			var dictd:Namespace = new Namespace("http://www.luetzschena-stahmeln.de/dictd/");
			var result:XML = event.result as XML;
			var server:String, description:String;
			var servers:Array = new Array();
			for each (var serverNode:XML in result.dictd::server)
			{
				server = serverNode.dictd::dictdurl;
				description = serverNode.dictd::description;
				if (StringUtil.trim(server).length != 0 &&
					StringUtil.trim(description).length != 0)
				{
					var dServer:DictionaryServer = new DictionaryServer();
					dServer.server = server.replace("dict://", "");
					dServer.description = description;
					servers.push(dServer);
				}
			}
			var dEvent:DictionaryServerEvent = new DictionaryServerEvent(DictionaryServerEvent.SERVERS);
			dEvent.servers = servers;
			dispatchEvent(dEvent);
		}

		private function incomingData(event:CompleteResponseEvent):void
		{			
			var rawResponse:String = event.response;
			var response:Response = this.parseRawResponse(rawResponse);
			var responseCode:uint = response.code;
			if (responseCode == 552) // no matches
			{
				throwNoMatchEvent(response);
			}
			else if (responseCode >= 400 && responseCode <= 599) // error
			{
				throwErrorEvent(response);
			}
			else if (responseCode == 220) // successful connection
			{
				dispatchEvent(new ConnectedEvent(ConnectedEvent.CONNECTED));
			}
			else if (responseCode == 110) // databases are being returned
			{
				throwDatabasesEvent(response);				
			}
			else if (responseCode == 111) // matches strategies
			{
				throwMatchStrategiesEvent(response);
			}
			else if (responseCode == 152) // matches
			{
				throwMatchEvent(response);
			}
			else if (responseCode == 150)
			{
				throwDefinitionHeaderEvent(response);
			}
			else if (responseCode == 151)
			{
				throwDefinitionEvent(response);
			}
    	}

    	private function ioError(event:IOErrorEvent):void
    	{
			dispatchEvent(event);
    	}

    	private function httpError(event:FaultEvent):void
    	{
    		trace("httpError!");
    	}

    	private function securityError(event:SecurityErrorEvent):void
    	{
    		trace("security error!");
    		trace(event.text);
    	}

    	// Dispatch new events.

    	private function throwDatabasesEvent(response:Response):void
    	{
			var databases:Array = new Array();
			var responseArray:Array = response.body.split("\r\n");
    		for each (var line:String in responseArray)
    		{
    			var name:String = line.substring(0, line.indexOf(" "));
    			if (name == "--exit--")
    			{
    				if (this.dbShortList)
    				{
    					break;
    				}
    				continue;
    			}
    			var description:String = line.substring(line.indexOf(" ")+1, line.length).replace(/\"/g,"");
    			databases.push(new Database(name, description));
    		}
    		var event:DatabaseEvent = new DatabaseEvent(DatabaseEvent.DATABASES);
    		event.databases = databases;
    		dispatchEvent(event);
    	}

    	private function throwMatchStrategiesEvent(response:Response):void
    	{
			var strategies:Array = new Array();
			var responseArray:Array = response.body.split("\r\n");
    		for each (var line:String in responseArray)
    		{
    			var name:String = line.substring(0, line.indexOf(" "));
    			var description:String = line.substring(line.indexOf(" ")+1, line.length).replace(/\"/g,"");
    			strategies.push(new MatchStrategy(name, description));
    		}
    		var event:MatchStrategiesEvent = new MatchStrategiesEvent(MatchStrategiesEvent.MATCH_STRATEGIES);
    		event.strategies = strategies;
    		dispatchEvent(event);
    	}

    	private function throwMatchEvent(response:Response):void
    	{
			var matches:Array = new Array();
			var responseArray:Array = response.body.split("\r\n");
    		for each (var line:String in responseArray)
    		{
    			var match:String = line.substring(line.indexOf(" ")+1, line.length).replace(/\"/g,"");
    			matches.push(match);
    		}
    		var event:MatchEvent = new MatchEvent(MatchEvent.MATCH);
    		event.matches = matches;
    		dispatchEvent(event);
    	}

    	private function throwErrorEvent(response:Response):void
    	{
    		var event:ErrorEvent = new ErrorEvent(ErrorEvent.ERROR);
    		event.code = response.code;
    		event.message = response.headerText;
			dispatchEvent(event);
    	}

    	private function throwNoMatchEvent(response:Response):void
    	{
			dispatchEvent(new NoMatchEvent(NoMatchEvent.NO_MATCH));
    	}

    	private function throwDefinitionHeaderEvent(response:Response):void
    	{
			var event:DefinitionHeaderEvent = new DefinitionHeaderEvent(DefinitionHeaderEvent.DEFINITION_HEADER);
			event.definitionCount = uint(response.headerText.substring(0, response.headerText.indexOf(" ")));
			dispatchEvent(event);
    	}

    	private function throwDefinitionEvent(response:Response):void
    	{
    		var event:DefinitionEvent = new DefinitionEvent(DefinitionEvent.DEFINITION);
    		var def:Definition = new Definition();
    		var headerText:String = response.headerText;
    		var tokens:Array = headerText.match(/"[^"]+"/g);
    		def.term = String(tokens[0]).replace(/"/g, "");
    		def.database = String(tokens[1]).replace(/"/g, "");
    		def.definition = response.body;
    		event.definition = def;
			dispatchEvent(event);
    	}

    	private function parseRawResponse(rawResponse:String):Response
    	{
    		var response:Response = new Response();
    		var fullHeader:String;
    		if (rawResponse.indexOf("\r\n") != -1)
    		{
	    		fullHeader = rawResponse.substring(0, rawResponse.indexOf("\r\n"));
    		}
    		else
    		{
    			fullHeader = rawResponse;
    		}
      		var responseCodeMatch:Array = fullHeader.match(/^\d{3}/);
    		response.code = uint(responseCodeMatch[0]);
    		response.headerText = fullHeader.substring(fullHeader.indexOf(" ")+1, fullHeader.length);
			var body:String = rawResponse.substring(rawResponse.indexOf("\r\n")+2, rawResponse.length);
			body = body.replace(/\r\n\.\./, "\r\n.");
			response.body = body;
    		return response;
    	}
	}
}