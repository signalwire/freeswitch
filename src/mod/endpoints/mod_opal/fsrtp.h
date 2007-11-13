
#ifndef __FREESWITCH_RTP_H_
#define __FREESWITCH_RTP_H_

#define HAVE_APR 
#include <switch.h>
#include <switch_version.h>
#include <ptlib.h>
#include <opal/buildopts.h>
#include <opal/connection.h>
#include <opal/rtp.h>

// keeps the freeswitch - opal rtp mapping
class FSRTPPairs : public PObject
{
	PCLASSINFO(FSRTPPairs, PObject);
	public:
		FSRTPPairs();
		~FSRTPPairs();

	private:
		switch_rtp_t	*fsrtp;
		RTP_UDP			*opalrtp;	
		
};

// fsrtp session is different from opalrtp session
// we just keep the rtp sessions in fsrtpsession
//
class FSRTPSession : public PObject
{
	PCLASSINFO(FSRTPSession, PObject);
	public:
		FSRTPSession(WORD port, BOOL isOpalRTP = FALSE);

	private:
		class RTPPairsDictionary : public PSafeDictionary<PString, FSRTPPairs>
		{
			virtual void DeleteObject(PObject * object) const;

		}rtpPairs;

};

#endif //__FREESWITCH_RTP_H_
