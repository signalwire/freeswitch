#ifndef __OPAL_H323_ENDPOINT__
#define __OPAL_H323_ENDPOINT__

#include <ptlib.h>
#include <opal/buildopts.h>
#include <opal/endpoint.h>
#include <h323/h323ep.h>
#include <h323/h323con.h>
#include "opal_h323con.h"



class FSH323EndPoint : public H323EndPoint
{
    PCLASSINFO(FSH323EndPoint, H323EndPoint);
    public:
        FSH323EndPoint(OpalManager & manager);
		~FSH323EndPoint();
		
		virtual OpalConnection::AnswerCallResponse OnAnswerCall(OpalConnection & connection,
																const PString & caller);
	
		virtual OpalMediaFormatList GetMediaFormats() const;
		
		H323Connection * CreateConnection(OpalCall & call,
                                                const PString & token,
                                                void * userData,
                                                OpalTransport & transport,
                                                const PString & alias,
                                                const H323TransportAddress & address,
                                                H323SignalPDU * setupPDU,
                                                unsigned options,
                                                OpalConnection::StringOptions * stringOptions);

		BOOL isExternalRTPEnabled(){return UseH323ExternalRTP;};
	
	protected:
		BOOL	UseH323ExternalRTP;
		OpalMediaFormatList mediaformats;
};


#endif //__OPAL_H323_ENDPOINT__
