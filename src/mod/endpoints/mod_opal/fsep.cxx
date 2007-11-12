

#include "fsep.h"
#include "fscon.h"


static PString MakeToken()
{
	return PGloballyUniqueID().AsString();
}

FSEndPoint::FSEndPoint(OpalManager & manager)
    :OpalEndPoint(manager, "fs", CanTerminateCall), initialized(TRUE), 
	currentmediaport(MIN_MEDIA_PORT), RTPLocalAddress(PIPSocket::Address::Address())
{

	
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
	PTRACE(3, "FSEndPoint: \t FSEndPoint Created!");
}

FSEndPoint::~FSEndPoint()
{

}

BOOL FSEndPoint::SetRTPAddress(PIPSocket::Address addr)
{
	RTPLocalAddress = addr;
	
	return TRUE;
}

PIPSocket::Address FSEndPoint::GetRTPAddress()
{
	return RTPLocalAddress;
}


BOOL FSEndPoint::OnOpenMediaStream(OpalConnection & connection, OpalMediaStream & stream)
{
	manager.OnOpenMediaStream(connection, stream);
	return TRUE;
}

BOOL FSEndPoint::OnIncomingConnection(OpalConnection&, unsigned int, OpalConnection::StringOptions*)
{
	return TRUE;
}

BOOL FSEndPoint::OnSetUpConnection(OpalConnection & connection)
{
	return TRUE;
}

void FSEndPoint::OnEstablished(OpalConnection &connection)
{
	
}

BOOL FSEndPoint::MakeConnection (OpalCall & call, const PString & party, 
								 void *userData, unsigned int options, 
								 OpalConnection::StringOptions* stringOptions)
{
	FSConnection *connection;
	PString token = MakeToken();
	connection = new FSConnection(call, *this, token, 0);
	PString dest = PString((const char*) userData);

	if (connection != NULL){

		if (!AddConnection(connection))
			return FALSE;
		
		if (call.GetConnection(0) == (OpalConnection*)connection){
			connection->SetUpConnection();
		}	
	}
	
	return TRUE;
}

OpalMediaFormatList FSEndPoint::GetMediaFormats() const
{
    return mediaformats;
}



WORD FSEndPoint::GetMediaPort()
{
	if (currentmediaport++ >= MAX_MEDIA_PORT) {
        currentmediaport = MIN_MEDIA_PORT;
    }

	return currentmediaport;	
}
