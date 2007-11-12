
#include "opal_h323.h"
#include "opal_h323con.h"

FSH323EndPoint::FSH323EndPoint(OpalManager & manager)
    :H323EndPoint(manager), UseH323ExternalRTP(TRUE)
{
	//if we have config option for using external rtp for h323, 
	//please set UseH323ExternalRTP to TRUE
	
	mediaformats+=OpalPCM16;
	mediaformats+=OpalG711uLaw;
	mediaformats+=OpalG711ALaw; 
	mediaformats+=OpalG711_ULAW_64K;
	mediaformats+=OpalG711_ALAW_64K;
	mediaformats+=OpalGSM0610;
	mediaformats+=OpalPCM16_16KHZ;
	mediaformats+=OPAL_G729;
	mediaformats+=OPAL_G729A;
	mediaformats+=OPAL_G729B;
	mediaformats+=OPAL_G729AB;
}

FSH323EndPoint::~FSH323EndPoint()
{

}


OpalMediaFormatList FSH323EndPoint::GetMediaFormats() const
{
	PTRACE(3, "GIMME MEDIA FORMAT");
	return mediaformats;
}


OpalConnection::AnswerCallResponse FSH323EndPoint::OnAnswerCall(OpalConnection & connection,
																const PString & caller)
{
	return OpalConnection::AnswerCallPending;
}


H323Connection * FSH323EndPoint::CreateConnection(OpalCall & call,
                                                const PString & token,
                                               void * userData,
                                               OpalTransport & transport,
                                                const PString & alias,
                                                const H323TransportAddress & address,
                                                H323SignalPDU * setupPDU,
                                               unsigned options,
                                               OpalConnection::StringOptions * stringOptions)
	
{
	if (UseH323ExternalRTP)
		return new FSH323Connection(call, *this, token, alias, address, options);
	
	return new H323Connection(call, *this, token, alias, address, options);
}
