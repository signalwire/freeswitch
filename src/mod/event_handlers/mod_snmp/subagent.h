#ifndef subagent_H
#define subagent_H

/* .1.3.6.1.4.1.27880.1.1 */
#define ID_VERSION_STR				1
#define ID_UUID						2

/* .1.3.6.1.4.1.27880.1.2 */
#define SS_UPTIME					1
#define SS_SESSIONS_SINCE_STARTUP	2
#define SS_CURRENT_SESSIONS			3
#define SS_MAX_SESSIONS				4
#define SS_CURRENT_CALLS			5
#define SS_SESSIONS_PER_SECOND		6
#define SS_MAX_SESSIONS_PER_SECOND	7


void init_subagent(void);
Netsnmp_Node_Handler handle_identity;
Netsnmp_Node_Handler handle_systemStats;

#endif /* subagent_H */
