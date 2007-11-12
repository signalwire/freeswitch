
#include "opal_sip.h"

FSSIPEndPoint::FSSIPEndPoint(OpalManager & manager)
    :SIPEndPoint(manager)
{

}

FSSIPEndPoint::~FSSIPEndPoint()
{

}

BOOL FSSIPEndPoint::OnIncomingConnection (OpalConnection &connection)
{
	const char *destination = connection.GetCalledDestinationNumber();
	
	
	PTRACE(3, "FSSIPEndPoint: Answering Incomming Call "<< "' To '" << destination << "'");
	
	GetManager().MakeConnection(connection.GetCall(), "fs:", (void *) destination);
	
	return TRUE;
}

OpalConnection::AnswerCallResponse FSSIPEndPoint::OnAnswerCall(OpalConnection & connection,
																const PString & caller)
{
	const char *destination = connection.GetCalledDestinationNumber();
	
	
	PTRACE(3, "FSSIPEndPoint: Answering Incomming Call from '" << caller << "' To '" << destination << "'");
	
	//OpalManager &manager = connection.GetEndPoint().GetManager();
	//OpalCall & call = connection.GetCall();
	
	
	//manager.MakeConnection(call, "fs", (void *) destination);
	
	return OpalConnection::AnswerCallPending;
}
