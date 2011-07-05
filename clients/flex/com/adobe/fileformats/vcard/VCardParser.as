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
package com.adobe.fileformats.vcard
{
	import mx.utils.Base64Decoder;
	import mx.utils.StringUtil;
	
	public class VCardParser
	{
		public static function parse(vcardStr:String):Array
		{
			var vcards:Array = new Array();
			var lines:Array = vcardStr.split(/\r\n/);
			var vcard:VCard;
			var type:String;
			var typeTmp:String;
			var line:String;

			for (var i:uint = 0; i < lines.length; ++i)
			{
				line = lines[i];
				if (line == "BEGIN:VCARD")
				{
					vcard = new VCard();
				}
				else if (line == "END:VCARD")
				{
					if (vcard != null)
					{
						vcards.push(vcard);
					}
				}
				else if(line.search(/^ORG/i) != -1)
				{
					var org:String = line.substring(4, line.length);
					var orgArray:Array = org.split(";");
					for each (var orgToken:String in orgArray)
					{
						if (StringUtil.trim(orgToken).length > 0)
						{
							vcard.orgs.push(orgToken);
						}
					}
				}
				else if(line.search(/^TITLE/i) != -1)
				{
					var title:String = line.substring(6, line.length);
					vcard.title = title;
				}
				else if (line.search(/^FN:/i) != -1)
				{
					var fullName:String = line.substring(3, line.length);
					vcard.fullName = fullName;
				}
				else if (line.search(/^TEL/i) != -1)
				{
					type = new String();
					typeTmp = new String();
					var phone:Phone = new Phone();
					var number:String;
					var phoneTokens:Array = line.split(";");
					for each (var phoneToken:String in phoneTokens)
					{
						if (phoneToken.search(/^TYPE=/i) != -1)
						{
							if (phoneToken.indexOf(":") != -1)
							{
								typeTmp = phoneToken.substring(5, phoneToken.indexOf(":"));
								number = phoneToken.substring(phoneToken.indexOf(":")+1, phoneToken.length);
							}
							else
							{									
								typeTmp = phoneToken.substring(5, phoneToken.length);
							}

							typeTmp = typeTmp.toLocaleLowerCase();

							if (typeTmp == "pref")
							{
								continue;
							}
							if (type.length != 0)
							{
								type += (" ");
							}
							type += typeTmp;
						}
					}
					if (type.length > 0 && number != null)
					{
						phone.type = type;
						phone.number = number;
					}
					vcard.phones.push(phone);
				}
				else if (line.search(/^EMAIL/i) != -1)
				{
					type = new String();
					typeTmp = new String();
					var email:Email = new Email();
					var emailAddress:String;
					var emailTokens:Array = line.split(";");
					for each (var emailToken:String in emailTokens)
					{
						if (emailToken.search(/^TYPE=/i) != -1)
						{
							if (emailToken.indexOf(":") != -1)
							{
								typeTmp = emailToken.substring(5, emailToken.indexOf(":"));
								emailAddress = emailToken.substring(emailToken.indexOf(":")+1, emailToken.length);
							}
							else
							{									
								typeTmp = emailToken.substring(5, emailToken.length);
							}

							typeTmp = typeTmp.toLocaleLowerCase();

							if (typeTmp == "pref" || typeTmp == "internet")
							{
								continue;
							}
							if (type.length != 0)
							{
								type += (" ");
							}
							type += typeTmp;
						}
					}
					if (type.length > 0 && emailAddress != null)
					{
						email.type = type;
						email.address = emailAddress;
					}
					vcard.emails.push(email);
				}
				else if (line.indexOf("ADR;") != -1)
				{
					var addressTokens:Array = line.split(";");
					var address:Address = new Address();
					for (var j:uint = 0; j < addressTokens.length; ++j)
					{
						var addressToken:String = addressTokens[j];
						if (addressToken.search(/^home:+$/i) != -1) // For Outlook, which uses non-standard vCards.
						{
							address.type = "home";
						}
						else if (addressToken.search(/^work:+$/i) != -1) // For Outlook, which uses non-standard vCards.
						{
							address.type = "work";
						}
						if (addressToken.search(/^type=/i) != -1)  // The "type" parameter is the standard way (which Address Book uses)
						{
							// First, remove the optional ":" character.
							addressToken = addressToken.replace(/:/,"");
							var addressType:String = addressToken.substring(5, addressToken.length).toLowerCase();
							if (addressType == "pref") // Not interested in which one is preferred.
							{
								continue;
							}
							else if (addressType.indexOf("home") != -1) // home
							{
								addressType = "home";
							}
							else if (addressType.indexOf("work") != -1) // work
							{
								addressType = "work";
							}
							else if (addressType.indexOf(",") != -1) // if the comma technique is used, just use the first one
							{
								var typeTokens:Array = addressType.split(",");
								for each (var typeToken:String in typeTokens)
								{
									if (typeToken != "pref")
									{
										addressType = typeToken;
										break;
									}
								}
							}
							address.type = addressType;
						}
						else if (addressToken.search(/^\d/) != -1 && address.street == null)
						{
							address.street = addressToken.replace(/\\n/, "");
							address.city = addressTokens[j+1];
							address.state = addressTokens[j+2];
							address.postalCode = addressTokens[j+3];
						}
					}
					if (address.type != null && address.street != null)
					{
						vcard.addresses.push(address);
					}

				}
				else if (line.search(/^PHOTO;BASE64/i) != -1)
				{
					var imageStr:String = new String();
					for (var k:uint = i+1; k < lines.length; ++k)
					{
						imageStr += lines[k];
						//if (lines[k].search(/.+\=$/) != -1) // Very slow in Mac due to RegEx bug
						if (lines[k].indexOf('=') != -1)
						{
							var decoder:Base64Decoder = new Base64Decoder();
							decoder.decode(imageStr);
							vcard.image = decoder.flush();
							break;
						}
					}
				}
			}
			return vcards;
		}
	}
}