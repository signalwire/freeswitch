/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2005 Jori Liesenborgs

  Contact: jori@lumumba.uhasselt.be

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.uhasselt.be), a research center of the Hasselt University
  (http://www.uhasselt.be). The library is based upon work done for 
  my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include "rtperrors.h"

#include "rtpdebug.h"

struct RTPErrorInfo
{
	int code;
	char *description;
};

static RTPErrorInfo ErrorDescriptions[]=
{
	{ ERR_RTP_OUTOFMEM,"Out of memory" },
	{ ERR_RTP_NOTHREADSUPPORT, "No JThread support was compiled in"},
	{ ERR_RTP_COLLISIONLIST_BADADDRESS, "Passed invalid address (null) to collision list"},
	{ ERR_RTP_HASHTABLE_ELEMENTALREADYEXISTS, "Element already exists in hash table"},
	{ ERR_RTP_HASHTABLE_ELEMENTNOTFOUND, "Element not found in hash table"},
	{ ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX, "Function returned an illegal hash index"},
	{ ERR_RTP_HASHTABLE_NOCURRENTELEMENT, "No current element selected in hash table"},
	{ ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX, "Function returned an illegal hash index"},
	{ ERR_RTP_KEYHASHTABLE_KEYALREADYEXISTS, "Key value already exists in key hash table"},
	{ ERR_RTP_KEYHASHTABLE_KEYNOTFOUND, "Key value not found in key hash table"},
	{ ERR_RTP_KEYHASHTABLE_NOCURRENTELEMENT, "No current element selected in key hash table"},
	{ ERR_RTP_PACKBUILD_ALREADYINIT, "RTP packet builder is already initialized"},
	{ ERR_RTP_PACKBUILD_CSRCALREADYINLIST, "The specified CSRC is already in the RTP packet builder's CSRC list"},
	{ ERR_RTP_PACKBUILD_CSRCLISTFULL, "The RTP packet builder's CSRC list already contains 15 entries"},
	{ ERR_RTP_PACKBUILD_CSRCNOTINLIST, "The specified CSRC was not found in the RTP packet builder's CSRC list"},
	{ ERR_RTP_PACKBUILD_DEFAULTMARKNOTSET, "The RTP packet builder's default mark flag is not set"},
	{ ERR_RTP_PACKBUILD_DEFAULTPAYLOADTYPENOTSET, "The RTP packet builder's default payload type is not set"},
	{ ERR_RTP_PACKBUILD_DEFAULTTSINCNOTSET, "The RTP packet builder's default timestamp increment is not set"},
	{ ERR_RTP_PACKBUILD_INVALIDMAXPACKETSIZE, "The specified maximum packet size for the RTP packet builder is invalid"},
	{ ERR_RTP_PACKBUILD_NOTINIT, "The RTP packet builder is not initialized"},
	{ ERR_RTP_PACKET_BADPAYLOADTYPE, "Invalid payload type"},
	{ ERR_RTP_PACKET_DATAEXCEEDSMAXSIZE, "Tried to create an RTP packet which whould exceed the specified maximum packet size"},
	{ ERR_RTP_PACKET_EXTERNALBUFFERNULL, "Illegal value (null) passed as external buffer for the RTP packet"},
	{ ERR_RTP_PACKET_ILLEGALBUFFERSIZE, "Illegal buffer size specified for the RTP packet"},
	{ ERR_RTP_PACKET_INVALIDPACKET, "Invalid RTP packet format"},
	{ ERR_RTP_PACKET_TOOMANYCSRCS, "More than 15 CSRCs specified for the RTP packet"},
	{ ERR_RTP_POLLTHREAD_ALREADYRUNNING, "Poll thread is already running"},
	{ ERR_RTP_POLLTHREAD_CANTINITMUTEX, "Can't initialize a mutex for the poll thread"},
	{ ERR_RTP_POLLTHREAD_CANTSTARTTHREAD, "Can't start the poll thread"},
	{ ERR_RTP_RTCPCOMPOUND_INVALIDPACKET, "Invalid RTCP compound packet format"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILDING, "Already building this RTCP compound packet"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYBUILT, "This RTCP compound packet is already built"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_ALREADYGOTREPORT, "There's already a SR or RR in this RTCP compound packet"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_APPDATALENTOOBIG, "The specified APP data length for the RTCP compound packet is too big"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_BUFFERSIZETOOSMALL, "The specified buffer size for the RTCP comound packet is too small"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALAPPDATALENGTH, "The APP data length must be a multiple of four"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_ILLEGALSUBTYPE, "The APP packet subtype must be smaller than 32"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_INVALIDITEMTYPE, "Invalid SDES item type specified for the RTCP compound packet"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_MAXPACKETSIZETOOSMALL, "The specified maximum packet size for the RTCP compound packet is too small"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_NOCURRENTSOURCE, "Tried to add an SDES item to the RTCP compound packet when no SSRC was present"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_NOREPORTPRESENT, "An RTCP compound packet must contain a SR or RR"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_NOTBUILDING, "The RTCP compound packet builder is not initialized"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_NOTENOUGHBYTESLEFT, "Adding this data would exceed the specified maximum RTCP compound packet size"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_REPORTNOTSTARTED, "Tried to add a report block to the RTCP compound packet when no SR or RR was started"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_TOOMANYSSRCS, "Only 31 SSRCs will fit into a BYE packet for the RTCP compound packet"},
	{ ERR_RTP_RTCPCOMPPACKBUILDER_TOTALITEMLENGTHTOOBIG, "The total data for the SDES PRIV item exceeds the maximum size (255 bytes) of an SDES item"},
	{ ERR_RTP_RTCPPACKETBUILDER_ALREADYINIT, "The RTCP packet builder is already initialized"},
	{ ERR_RTP_RTCPPACKETBUILDER_ILLEGALMAXPACKSIZE, "The specified maximum packet size for the RTCP packet builder is too small"},
	{ ERR_RTP_RTCPPACKETBUILDER_ILLEGALTIMESTAMPUNIT, "Speficied an illegal timestamp unit for the the RTCP packet builder"},
	{ ERR_RTP_RTCPPACKETBUILDER_NOTINIT, "The RTCP packet builder was not initialized"},
	{ ERR_RTP_RTCPPACKETBUILDER_PACKETFILLEDTOOSOON, "The RTCP compound packet filled sooner than expected"},
	{ ERR_RTP_SCHEDPARAMS_BADFRACTION, "Illegal sender bandwidth fraction specified"},
	{ ERR_RTP_SCHEDPARAMS_BADMINIMUMINTERVAL, "The minimum RTCP interval specified for the scheduler is too small"},
	{ ERR_RTP_SCHEDPARAMS_INVALIDBANDWIDTH, "Invalid RTCP bandwidth specified for the RTCP scheduler"},
	{ ERR_RTP_SDES_LENGTHTOOBIG, "Specified size for the SDES item exceeds 255 bytes"},
	{ ERR_RTP_SDES_PREFIXNOTFOUND, "The specified SDES PRIV prefix was not found"},
	{ ERR_RTP_SESSION_ALREADYCREATED, "The session is already created"},
	{ ERR_RTP_SESSION_CANTGETLOGINNAME, "Can't retrieve login name"},
	{ ERR_RTP_SESSION_CANTINITMUTEX, "A mutex for the RTP session couldn't be initialized"},
	{ ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL, "The maximum packet size specified for the RTP session is too small"},
	{ ERR_RTP_SESSION_NOTCREATED, "The RTP session was not created"},
	{ ERR_RTP_SESSION_UNSUPPORTEDTRANSMISSIONPROTOCOL, "The requested transmission protocol for the RTP session is not supported"},
	{ ERR_RTP_SESSION_USINGPOLLTHREAD, "This function is not available when using the RTP poll thread feature"},
	{ ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL, "A user-defined transmitter was requested but the supplied transmitter component is NULL"},
	{ ERR_RTP_SOURCES_ALREADYHAVEOWNSSRC, "Only one source can be marked as own SSRC in the source table"},
	{ ERR_RTP_SOURCES_DONTHAVEOWNSSRC, "No source was marked as own SSRC in the source table"},
	{ ERR_RTP_SOURCES_ILLEGALSDESTYPE, "Illegal SDES type specified for processing into the source table"},
	{ ERR_RTP_SOURCES_SSRCEXISTS, "Can't create own SSRC because this SSRC identifier is already in the source table"},
	{ ERR_RTP_UDPV4TRANS_ALREADYCREATED, "The transmitter was already created"},
	{ ERR_RTP_UDPV4TRANS_ALREADYINIT, "The transmitter was already initialize"},
	{ ERR_RTP_UDPV4TRANS_ALREADYWAITING, "The transmitter is already waiting for incoming data"},
	{ ERR_RTP_UDPV4TRANS_CANTBINDRTCPSOCKET, "The 'bind' call for the RTCP socket failed"},
	{ ERR_RTP_UDPV4TRANS_CANTBINDRTPSOCKET, "The 'bind' call for the RTP socket failed"},
	{ ERR_RTP_UDPV4TRANS_CANTCALCULATELOCALIP, "The local IP addresses could not be determined"},
	{ ERR_RTP_UDPV4TRANS_CANTCREATEABORTDESCRIPTORS, "Couldn't create the sockets used to abort waiting for incoming data"},
	{ ERR_RTP_UDPV4TRANS_CANTCREATEPIPE, "Couldn't create the pipe used to abort waiting for incoming data"},
	{ ERR_RTP_UDPV4TRANS_CANTCREATESOCKET, "Couldn't create the RTP or RTCP socket"},
	{ ERR_RTP_UDPV4TRANS_CANTINITMUTEX, "Failed to initialize a mutex used by the transmitter"},
	{ ERR_RTP_UDPV4TRANS_CANTSETRTCPRECEIVEBUF, "Couldn't set the receive buffer size for the RTCP socket"},
	{ ERR_RTP_UDPV4TRANS_CANTSETRTCPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTCP socket"},
	{ ERR_RTP_UDPV4TRANS_CANTSETRTPRECEIVEBUF, "Couldn't set the receive buffer size for the RTP socket"},
	{ ERR_RTP_UDPV4TRANS_CANTSETRTPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTP socket"},
	{ ERR_RTP_UDPV4TRANS_COULDNTJOINMULTICASTGROUP, "Unable to join the specified multicast group"},
	{ ERR_RTP_UDPV4TRANS_DIFFERENTRECEIVEMODE, "The function called doens't match the current receive mode"},
	{ ERR_RTP_UDPV4TRANS_ERRORINSELECT, "Error in the transmitter's 'select' call"},
	{ ERR_RTP_UDPV4TRANS_ILLEGALPARAMETERS, "Illegal parameters type passed to the transmitter"},
	{ ERR_RTP_UDPV4TRANS_INVALIDADDRESSTYPE, "Specified address type isn't compatible with this transmitter"},
	{ ERR_RTP_UDPV4TRANS_NOLOCALIPS, "Couldn't determine the local host name since the local IP list is empty"},
	{ ERR_RTP_UDPV4TRANS_NOMULTICASTSUPPORT, "Multicast support is not available"},
	{ ERR_RTP_UDPV4TRANS_NOSUCHENTRY, "Specified entry could not be found"},
	{ ERR_RTP_UDPV4TRANS_NOTAMULTICASTADDRESS, "The specified address is not a multicast address"},
	{ ERR_RTP_UDPV4TRANS_NOTCREATED, "The 'Create' call for this transmitter has not been called"},
	{ ERR_RTP_UDPV4TRANS_NOTINIT, "The 'Init' call for this transmitter has not been called"},
	{ ERR_RTP_UDPV4TRANS_NOTWAITING, "The transmitter is not waiting for incoming data"},
	{ ERR_RTP_UDPV4TRANS_PORTBASENOTEVEN, "The specified port base is not an even number"},
	{ ERR_RTP_UDPV4TRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size is too big for this transmitter"},
	{ ERR_RTP_UDPV6TRANS_ALREADYCREATED, "The transmitter was already created"},
	{ ERR_RTP_UDPV6TRANS_ALREADYINIT, "The transmitter was already initialize"},
	{ ERR_RTP_UDPV6TRANS_ALREADYWAITING, "The transmitter is already waiting for incoming data"},
	{ ERR_RTP_UDPV6TRANS_CANTBINDRTCPSOCKET, "The 'bind' call for the RTCP socket failed"},
	{ ERR_RTP_UDPV6TRANS_CANTBINDRTPSOCKET, "The 'bind' call for the RTP socket failed"},
	{ ERR_RTP_UDPV6TRANS_CANTCALCULATELOCALIP, "The local IP addresses could not be determined"},
	{ ERR_RTP_UDPV6TRANS_CANTCREATEABORTDESCRIPTORS, "Couldn't create the sockets used to abort waiting for incoming data"},
	{ ERR_RTP_UDPV6TRANS_CANTCREATEPIPE, "Couldn't create the pipe used to abort waiting for incoming data"},
	{ ERR_RTP_UDPV6TRANS_CANTCREATESOCKET, "Couldn't create the RTP or RTCP socket"},
	{ ERR_RTP_UDPV6TRANS_CANTINITMUTEX, "Failed to initialize a mutex used by the transmitter"},
	{ ERR_RTP_UDPV6TRANS_CANTSETRTCPRECEIVEBUF, "Couldn't set the receive buffer size for the RTCP socket"},
	{ ERR_RTP_UDPV6TRANS_CANTSETRTCPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTCP socket"},
	{ ERR_RTP_UDPV6TRANS_CANTSETRTPRECEIVEBUF, "Couldn't set the receive buffer size for the RTP socket"},
	{ ERR_RTP_UDPV6TRANS_CANTSETRTPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTP socket"},
	{ ERR_RTP_UDPV6TRANS_COULDNTJOINMULTICASTGROUP, "Unable to join the specified multicast group"},
	{ ERR_RTP_UDPV6TRANS_DIFFERENTRECEIVEMODE, "The function called doens't match the current receive mode"},
	{ ERR_RTP_UDPV6TRANS_ERRORINSELECT, "Error in the transmitter's 'select' call"},
	{ ERR_RTP_UDPV6TRANS_ILLEGALPARAMETERS, "Illegal parameters type passed to the transmitter"},
	{ ERR_RTP_UDPV6TRANS_INVALIDADDRESSTYPE, "Specified address type isn't compatible with this transmitter"},
	{ ERR_RTP_UDPV6TRANS_NOLOCALIPS, "Couldn't determine the local host name since the local IP list is empty"},
	{ ERR_RTP_UDPV6TRANS_NOMULTICASTSUPPORT, "Multicast support is not available"},
	{ ERR_RTP_UDPV6TRANS_NOSUCHENTRY, "Specified entry could not be found"},
	{ ERR_RTP_UDPV6TRANS_NOTAMULTICASTADDRESS, "The specified address is not a multicast address"},
	{ ERR_RTP_UDPV6TRANS_NOTCREATED, "The 'Create' call for this transmitter has not been called"},
	{ ERR_RTP_UDPV6TRANS_NOTINIT, "The 'Init' call for this transmitter has not been called"},
	{ ERR_RTP_UDPV6TRANS_NOTWAITING, "The transmitter is not waiting for incoming data"},
	{ ERR_RTP_UDPV6TRANS_PORTBASENOTEVEN, "The specified port base is not an even number"},
	{ ERR_RTP_UDPV6TRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size is too big for this transmitter"},
	{ ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL,"The hostname is larger than the specified buffer size"},
	{ ERR_RTP_SDES_MAXPRIVITEMS,"The maximum number of SDES private item prefixes was reached"},
	{ ERR_RTP_INTERNALSOURCEDATA_INVALIDPROBATIONTYPE,"An invalid probation type was specified"},
	{ ERR_RTP_GSTV4TRANS_ALREADYCREATED, "The transmitter was already created"},
	{ ERR_RTP_GSTV4TRANS_ALREADYINIT, "The transmitter was already initialize"},
	{ ERR_RTP_GSTV4TRANS_ALREADYWAITING, "The transmitter is already waiting for incoming data"},
	{ ERR_RTP_GSTV4TRANS_CANTBINDRTCPSOCKET, "The 'bind' call for the RTCP socket failed"},
	{ ERR_RTP_GSTV4TRANS_CANTBINDRTPSOCKET, "The 'bind' call for the RTP socket failed"},
	{ ERR_RTP_GSTV4TRANS_CANTCALCULATELOCALIP, "The local IP addresses could not be determined"},
	{ ERR_RTP_GSTV4TRANS_CANTCREATEABORTDESCRIPTORS, "Couldn't create the sockets used to abort waiting for incoming data"},
	{ ERR_RTP_GSTV4TRANS_CANTCREATEPIPE, "Couldn't create the pipe used to abort waiting for incoming data"},
	{ ERR_RTP_GSTV4TRANS_CANTCREATESOCKET, "Couldn't create the RTP or RTCP socket"},
	{ ERR_RTP_GSTV4TRANS_CANTINITMUTEX, "Failed to initialize a mutex used by the transmitter"},
	{ ERR_RTP_GSTV4TRANS_CANTSETRTCPRECEIVEBUF, "Couldn't set the receive buffer size for the RTCP socket"},
	{ ERR_RTP_GSTV4TRANS_CANTSETRTCPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTCP socket"},
	{ ERR_RTP_GSTV4TRANS_CANTSETRTPRECEIVEBUF, "Couldn't set the receive buffer size for the RTP socket"},
	{ ERR_RTP_GSTV4TRANS_CANTSETRTPTRANSMITBUF, "Couldn't set the transmission buffer size for the RTP socket"},
	{ ERR_RTP_GSTV4TRANS_COULDNTJOINMULTICASTGROUP, "Unable to join the specified multicast group"},
	{ ERR_RTP_GSTV4TRANS_DIFFERENTRECEIVEMODE, "The function called doens't match the current receive mode"},
	{ ERR_RTP_GSTV4TRANS_ERRORINSELECT, "Error in the transmitter's 'select' call"},
	{ ERR_RTP_GSTV4TRANS_ILLEGALPARAMETERS, "Illegal parameters type passed to the transmitter"},
	{ ERR_RTP_GSTV4TRANS_INVALIDADDRESSTYPE, "Specified address type isn't compatible with this transmitter"},
	{ ERR_RTP_GSTV4TRANS_NOLOCALIPS, "Couldn't determine the local host name since the local IP list is empty"},
	{ ERR_RTP_GSTV4TRANS_NOMULTICASTSUPPORT, "Multicast support is not available"},
	{ ERR_RTP_GSTV4TRANS_NOSUCHENTRY, "Specified entry could not be found"},
	{ ERR_RTP_GSTV4TRANS_NOTAMULTICASTADDRESS, "The specified address is not a multicast address"},
	{ ERR_RTP_GSTV4TRANS_NOTCREATED, "The 'Create' call for this transmitter has not been called"},
	{ ERR_RTP_GSTV4TRANS_NOTINIT, "The 'Init' call for this transmitter has not been called"},
	{ ERR_RTP_GSTV4TRANS_NOTWAITING, "The transmitter is not waiting for incoming data"},
	{ ERR_RTP_GSTV4TRANS_PORTBASENOTEVEN, "The specified port base is not an even number"},
	{ ERR_RTP_GSTV4TRANS_SPECIFIEDSIZETOOBIG, "The maximum packet size is too big for this transmitter"},
	{ ERR_RTP_GSTV4TRANS_INVALIDEVENT, "Expecting UNKNOWN_EVENT to set source address but got another type of event"},
	{ ERR_RTP_GSTV4TRANS_SRCADDRNOTSET, "Got packet but src address information was not set, returning"},
	{ ERR_RTP_GSTV4TRANS_NOTNETBUFFER, "Received buffer is not a GstNetBuffer"},
	{ ERR_RTP_GSTV4TRANS_WAITNOTIMPLEMENTED, "The WaitForIncomingData is not implemented in the Gst transmitter"},
	{ 0,0 }
};

std::string RTPGetErrorString(int errcode)
{
	int i;
	
	if (errcode >= 0)
		return std::string("No error");
	
	i = 0;
	while (ErrorDescriptions[i].code != 0)
	{
		if (ErrorDescriptions[i].code == errcode)
			return std::string(ErrorDescriptions[i].description);
		i++;
	}
	return std::string("Unknown error code");
}

