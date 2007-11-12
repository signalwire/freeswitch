
#include "fsmanager.h"

FSManager::FSManager()
	:SessionsHashTable(NULL), SessionsHashTableMutex(NULL),
	h323ep(NULL), fsep(NULL)
{
    
}

FSManager::~FSManager()
{    
	switch_mutex_destroy(SessionsHashTableMutex);
	switch_core_hash_destroy(&SessionsHashTable);
}

BOOL FSManager::Initialize(switch_memory_pool_t* MemoryPool)
{
	silenceDetectParams.m_mode = OpalSilenceDetector::NoSilenceDetection;
	SetAudioJitterDelay(800, 3000);
	
	if(switch_core_hash_init(&SessionsHashTable,MemoryPool)!=SWITCH_STATUS_SUCCESS)
    {        
        assert(0);
        return FALSE;
    }
    
    if(switch_mutex_init(&SessionsHashTableMutex,SWITCH_MUTEX_UNNESTED,MemoryPool)!=SWITCH_STATUS_SUCCESS)
    {
       assert(0);
       switch_core_hash_destroy(&SessionsHashTable);     
       return FALSE; 
    }
	
	sipep = new FSSIPEndPoint( *(static_cast<OpalManager*>(this)));
	h323ep = new FSH323EndPoint( *(static_cast<OpalManager*>(this)));
	fsep = new FSEndPoint( *(static_cast<OpalManager*>(this)));
	
	PIPSocket::Address addr("10.0.0.1");
	
	OpalTransportAddress sipTransportAddress(addr,5060, "udp");
	OpalTransportAddress h323TransportAddress(addr,1726);
	
	
	fsep->SetRTPAddress(addr);
	
	if(!sipep->StartListeners(sipTransportAddress)){
		delete sipep;
		return FALSE;
	}
	
	if(!h323ep->StartListeners(h323TransportAddress)){
		delete h323ep;
		delete sipep;
		return FALSE;
	}
	SetSTUNServer("stun.voxgratia.org");
	PStringArray routes;
	routes += "h323:.* = sip:<da>@fwd.pulver.com";
	SetRouteTable(routes);
	
	return TRUE;
}
