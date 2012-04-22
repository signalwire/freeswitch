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

package com.adobe.utils
{

	public class XMLUtil
	{
		/**
		 * Constant representing a text node type returned from XML.nodeKind.
		 * 
		 * @see XML.nodeKind()
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */
		public static const TEXT:String = "text";
		
		/**
		 * Constant representing a comment node type returned from XML.nodeKind.
		 * 
		 * @see XML.nodeKind()
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */		
		public static const COMMENT:String = "comment";
		
		/**
		 * Constant representing a processing instruction type returned from XML.nodeKind.
		 * 
		 * @see XML.nodeKind()
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */		
		public static const PROCESSING_INSTRUCTION:String = "processing-instruction";
		
		/**
		 * Constant representing an attribute type returned from XML.nodeKind.
		 * 
		 * @see XML.nodeKind()
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */		
		public static const ATTRIBUTE:String = "attribute";
		
		/**
		 * Constant representing a element type returned from XML.nodeKind.
		 * 
		 * @see XML.nodeKind()
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */		
		public static const ELEMENT:String = "element";
		
		/**
		 * Checks whether the specified string is valid and well formed XML.
		 * 
		 * @param data The string that is being checked to see if it is valid XML.
		 * 
		 * @return A Boolean value indicating whether the specified string is
		 * valid XML.
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */
		public static function isValidXML(data:String):Boolean
		{
			var xml:XML;
			
			try
			{
				xml = new XML(data);
			}
			catch(e:Error)
			{
				return false;
			}
			
			if(xml.nodeKind() != XMLUtil.ELEMENT)
			{
				return false;
			}
			
			return true;
		}
		
		/**
		 * Returns the next sibling of the specified node relative to the node's parent.
		 * 
		 * @param x The node whose next sibling will be returned.
		 * 
		 * @return The next sibling of the node. null if the node does not have 
		 * a sibling after it, or if the node has no parent.
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */		
		public static function getNextSibling(x:XML):XML
		{	
			return XMLUtil.getSiblingByIndex(x, 1);
		}
		
		/**
		 * Returns the sibling before the specified node relative to the node's parent.
		 * 
		 * @param x The node whose sibling before it will be returned.
		 * 
		 * @return The sibling before the node. null if the node does not have 
		 * a sibling before it, or if the node has no parent.
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */			
		public static function getPreviousSibling(x:XML):XML
		{	
			return XMLUtil.getSiblingByIndex(x, -1);
		}		
		
		protected static function getSiblingByIndex(x:XML, count:int):XML	
		{
			var out:XML;
			
			try
			{
				out = x.parent().children()[x.childIndex() + count];	
			} 		
			catch(e:Error)
			{
				return null;
			}
			
			return out;			
		}
	}
}