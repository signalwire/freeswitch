

#ifndef __FREESWITCH_OPAL_ENDPOINT__
#define __FREESWITCH_OPAL_ENDPOINT__

#include <switch.h>
#include <ptlib.h>
#include <opal/buildopts.h>
#include <opal/endpoint.h>

#define MAX_MEDIA_PORT 44526
#define MIN_MEDIA_PORT 44000

class FSEndPoint : public OpalEndPoint
{
    PCLASSINFO(FSEndPoint, OpalEndPoint);
    public:
        FSEndPoint(OpalManager& manager);
        ~FSEndPoint();

        virtual BOOL OnIncomingConnection(OpalConnection&, unsigned int, OpalConnection::StringOptions*);

        virtual BOOL MakeConnection(OpalCall & call, const PString & party,
                                    void * userData, unsigned int options = 0,
                                    OpalConnection::StringOptions* stringOptions = NULL);

		virtual BOOL OnOpenMediaStream(OpalConnection & connection, OpalMediaStream & stream);
		virtual BOOL OnSetUpConnection(OpalConnection & connection);
		virtual void OnEstablished(OpalConnection &connection);
        virtual OpalMediaFormatList GetMediaFormats() const;
		
		WORD GetMediaPort();
		BOOL SetRTPAddress(PIPSocket::Address ipaddr);
		PIPSocket::Address GetRTPAddress();

	private:
		BOOL initialized;
		PINDEX maxcalls;
		OpalMediaFormatList  mediaformats;
		WORD	currentmediaport;
		PIPSocket::Address RTPLocalAddress;

};

#endif //__FREESWITCH_OPAL_ENDPOINT__
