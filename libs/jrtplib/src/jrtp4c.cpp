/*
 * CCRTP4C (A wrapper for ccRTP so you can use it in C programs)
 * Copyright Anthony Minessale II <anthmct@yahoo.com>
 *
 */
#include <cstdio>
#include <ctime>
#include <jrtp4c.h>

#ifdef WIN32
static int WSOCKON = 0;
#endif


#ifdef __cplusplus
extern "C" {
#endif

	class JRTP4C: public RTPSession  {
		// add virtuals later 
	public:
		JRTP4C(): RTPSession() {}
	};

	struct jrtp4c {
		JRTP4C *session;
		RTPUDPv4TransmissionParams *transparams;
		uint32_t ssrc;
		int payload;
	};

	struct jrtp4c *jrtp4c_new(char *rx_ip, int rx_port, char *tx_ip, int tx_port, int payload, int sps, const char **err)
	{
		struct jrtp4c *jrtp4c = NULL;
		uint32_t destip =  ntohl(inet_addr(tx_ip));
		uint32_t srcip;
		RTPIPv4Address addr(destip, tx_port);
		RTPSessionParams sessparams;
		int status;
		
#ifdef WIN32
		if (!WSOCKON) {
			WSADATA dat;
			WSAStartup(MAKEWORD(2,2),&dat);
			WSOCKON = 1;
		}
#endif // WIN32

		sessparams.SetOwnTimestampUnit(1.0/sps);		
		//sessparams.SetAcceptOwnPackets(true);

		if (!(jrtp4c = new struct jrtp4c)) {
			*err = "Memory Error!\n";
			memset(jrtp4c, 0, sizeof(*jrtp4c));
			return NULL;
		}

		if (!(jrtp4c->transparams = new RTPUDPv4TransmissionParams)) {
			delete jrtp4c;
			*err = "Memory Error!\n";
			return NULL;
		}


		if (rx_ip) {
			srcip = ntohl(inet_addr(rx_ip));
			jrtp4c->transparams->SetBindIP(srcip);
		}

		jrtp4c->transparams->SetPortbase(rx_port);


		

		if (!(jrtp4c->session = new JRTP4C)) {
			*err = "Memory Error!\n";
			delete jrtp4c->transparams;
            delete jrtp4c;
            return NULL;
		}

		if ((status = jrtp4c->session->Create(sessparams, jrtp4c->transparams)) < 0) {
			*err = RTPGetErrorString(status).c_str();
			delete jrtp4c->transparams;
			delete jrtp4c;
			return NULL;
		}


		if ((status = jrtp4c->session->AddDestination(addr)) < 0) {
			*err = RTPGetErrorString(status).c_str();
			jrtp4c_destroy(&jrtp4c);
			return NULL;
		}

		jrtp4c->session->SetDefaultPayloadType(payload);		
		jrtp4c->payload = payload;
		return jrtp4c;
	}

	void jrtp4c_destroy(struct jrtp4c **jrtp4c)
	{
		(*jrtp4c)->session->BYEDestroy(RTPTime(10,0),0,0);
		delete (*jrtp4c)->session;
		delete (*jrtp4c)->transparams;
		delete (*jrtp4c);
		*jrtp4c = NULL;
	}


	void jrtp4c_killread(struct jrtp4c *jrtp4c)
	{
		jrtp4c->session->AbortWait();
	}

	int jrtp4c_read(struct jrtp4c *jrtp4c, void *data, int datalen, int *payload_type)
	{
		RTPPacket *pack;
		int slen = 0;
		bool hasdata = 0;

		*payload_type = 0;

		jrtp4c->session->BeginDataAccess();

		jrtp4c->session->WaitForIncomingData(RTPTime(.5), &hasdata);

		if (!jrtp4c->session->GotoFirstSourceWithData()) {
			jrtp4c->session->EndDataAccess();
			return 0;
		}

		
		if ((pack = jrtp4c->session->GetNextPacket())) {
			slen = pack->GetPayloadLength();

			if (slen > datalen) {
				slen = datalen;
			}

		
			*payload_type = pack->GetPayloadType();

			memcpy(data, pack->GetPayloadData(), slen);

			delete pack;
		}
		jrtp4c->session->EndDataAccess();

		return slen;

	}

	int jrtp4c_write(struct jrtp4c *jrtp4c, void *data, int datalen, uint32_t ts)
	{
		return jrtp4c->session->SendPacket(data, datalen, jrtp4c->payload, false, ts);
	}

	uint32_t jrtp4c_start(struct jrtp4c *jrtp4c)
	{
		//jrtp4c->session->BeginDataAccess();
		return 0;
	}

	uint32_t jrtp4c_get_ssrc(struct jrtp4c *jrtp4c)
	{
		return jrtp4c->ssrc;
	}

#ifdef __cplusplus
}
#endif
 
/** EMACS **
 * Local variables:
 * mode: c++
 * c-basic-offset: 4
 * End:
 */
