
#ifndef __FREESWITCH_OPAL_MANAGER__
#define __FREESWITCH_OPAL_MANAGER__

#include <ptlib.h>
#include <opal/manager.h>
#include <h323/h323ep.h>
#include "fsep.h"
#include "opal_h323.h"
#include "opal_sip.h"

class FSH323EndPoint;

class FSManager : public OpalManager
{
    PCLASSINFO(FSManager, PObject);
    public:
		FSManager();
		~FSManager();
		
		BOOL Initialize(switch_memory_pool_t* MemoryPool);
	
	private:
		switch_hash_t			*SessionsHashTable;
		switch_mutex_t  		*SessionsHashTableMutex;
		switch_memory_pool_t	*MemoryPool;
	
	protected:
		FSH323EndPoint *h323ep;
		FSSIPEndPoint	*sipep;
		FSEndPoint   *fsep;
};		

#endif //__FREESWITCH_OPAL_MANAGER__
