#ifndef subagent_H
#define subagent_H

/* .1.3.6.1.4.1.27880.1.1 */
#define ID_VERSION_STR			1
#define ID_UUID				2

/* .1.3.6.1.4.1.27880.1.2 */
#define SS_UPTIME			1
#define SS_SESSIONS_SINCE_STARTUP	2
#define SS_CURRENT_SESSIONS		3
#define SS_MAX_SESSIONS			4
#define SS_CURRENT_CALLS		5
#define SS_SESSIONS_PER_SECOND		6
#define SS_MAX_SESSIONS_PER_SECOND	7

/* .1.3.6.1.4.1.27880.1.9 */
#define CH_UUID				1
#define CH_DIRECTION			2
#define CH_CREATED			3
#define CH_NAME				4
#define CH_STATE			5
#define CH_CID_NAME			6
#define CH_CID_NUM			7

typedef struct {
	uint32_t idx;
	char uuid[38];
	char direction[32];
	char created[128];
	char name[1024];
	char state[64];
	char cid_name[1024];
	char cid_num[256];
} chan_entry_t;

void init_subagent(switch_memory_pool_t *pool);
Netsnmp_Node_Handler handle_identity;
Netsnmp_Node_Handler handle_systemStats;
Netsnmp_Node_Handler handle_channelList;

#endif /* subagent_H */
