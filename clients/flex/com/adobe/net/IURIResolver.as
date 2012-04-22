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
	/**
	 * The URI class cannot know about DNS aliases, virtual hosts, or
	 * symbolic links that may be involved.  The application can provide
	 * an implementation of this interface to resolve the URI before the
	 * URI class makes any comparisons.  For example, a web host has
	 * two aliases:
	 * 
	 * <p><code>
	 *    http://www.site.com/
	 *    http://www.site.net/
	 * </code></p>
	 * 
	 * <p>The application can provide an implementation that automatically
	 * resolves site.net to site.com before URI compares two URI objects.
	 * Only the application can know and understand the context in which
	 * the URI's are being used.</p>
	 * 
	 * <p>Use the URI.resolver accessor to assign a custom resolver to
	 * the URI class.  Any resolver specified is global to all instances
	 * of URI.</p>
	 * 
	 * <p>URI will call this before performing URI comparisons in the
	 * URI.getRelation() and URI.getCommonParent() functions.
	 * 
	 * @see URI.getRelation
	 * @see URI.getCommonParent
	 * 
	 * @langversion ActionScript 3.0
	 * @playerversion Flash 9.0
	 */
	public interface IURIResolver
	{
		/**
		 * Implement this method to provide custom URI resolution for
		 * your application.
		 * 
		 * @langversion ActionScript 3.0
		 * @playerversion Flash 9.0
		 */
		function resolve(uri:URI) : URI;
	}
}