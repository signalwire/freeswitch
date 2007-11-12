
#ifndef __OPAL_SIP_ENDPOINT__
#define __OPAL_SIP_ENDPOINT__

#include <ptlib.h>
#include <opal/buildopts.h>
#include <opal/endpoint.h>
#include <sip/sipep.h>



class FSSIPEndPoint : public SIPEndPoint
{
    PCLASSINFO(FSSIPEndPoint, SIPEndPoint);
    public:
        FSSIPEndPoint(OpalManager & manager);
		~FSSIPEndPoint();
	
		virtual BOOL OnIncomingConnection (OpalConnection &connection);
		virtual OpalConnection::AnswerCallResponse OnAnswerCall(OpalConnection & connection,
																const PString & caller);
};


#endif //__OPAL_SIP_ENDPOINT__
