#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#include "../src/include/ks_rpcmessage.h"

#pragma GCC optimize ("O0")


ks_pool_t *pool;
ks_thread_pool_t *tpool;

ks_rpcmessaging_handle_t *h;

static ks_thread_t *threads[10];

static char idbuffer[51];


static ks_status_t  process_wombat_response(ks_rpcmessaging_handle_t* handle, cJSON *msg)
{
	   printf("entering process_wombat_response\n");
	   printf("exiting process_wombat_response\n");
		return KS_STATUS_FAIL;
}

static ks_status_t  process_wombat(ks_rpcmessaging_handle_t* handle, cJSON *msg, cJSON **response)
{
	printf("entering process_wombat\n");
	
	char *b0 = cJSON_Print(msg);
	printf("Request: %s\n", b0);
	free(b0);

	cJSON *msg_id = cJSON_GetObjectItem(msg, "id");
	if (msg_id) {
		if (msg_id->type == cJSON_Number) {
			printf("Number int %d double %f\n", msg_id->valueint, msg_id->valuedouble);
		}
	}
	
	cJSON *respvalue = cJSON_CreateNumber(1);

	ks_rpcmessage_id msgid = ks_rpcmessage_create_response(h, msg, &respvalue, response);

    char *b1 = cJSON_Print(*response);   //(*response);
    printf("Response: msgid %d\n%s\n", msgid, b1);
    free(b1);

    printf("exiting process_wombat\n");

	return KS_STATUS_SUCCESS; 
}

static ks_status_t  process_badbunny(ks_rpcmessaging_handle_t* handle, cJSON *msg, cJSON **response)
{
    printf("entering process_badbunny\n");

    char *b0 = cJSON_Print(msg);
    printf("Request: %s\n", b0);
    free(b0);

    cJSON *msg_id = cJSON_GetObjectItem(msg, "id");
    if (msg_id) {
        if (msg_id->type == cJSON_Number) {
            printf("Number int %d double %f\n", msg_id->valueint, msg_id->valuedouble);
        }
    }

    cJSON *respvalue;

    ks_rpcmessage_id msgid = ks_rpcmessage_create_errorresponse(h, msg, &respvalue, response);

    char *b2 = cJSON_Print(*response);
    printf("Request: msgid %d\n%s\n", msgid, b2);
    free(b2);

	//cJSON *respvalue = cJSON_CreateNumber(1);
    	

    char *b1 = cJSON_Print(*response);   //(*response);
    printf("Response: %s\n", b1);
    free(b1);

    printf("exiting process_badbunny\n");


    return KS_STATUS_SUCCESS;
}


void test01()
{
	printf("**** testrpcmessages - test01 start\n"); fflush(stdout);

	ks_rpcmessage_register_function(h, "wombat", process_wombat, process_wombat_response); 
    ks_rpcmessage_register_function(h, "badbunny", process_badbunny, NULL);
    ks_rpcmessage_register_function(h, "onewaywombat", NULL, process_wombat_response);

	cJSON* request = NULL;
	cJSON* parms   = NULL;
    cJSON* response  = NULL;

	/* try an invalid message */

	ks_rpcmessage_id msgid = ks_rpcmessage_create_request(h, "colm", &parms, &request);
	if (msgid != 0) {
		printf("invalid message created %d\n", msgid);
		printf("request:\n%s\n", cJSON_Print(request));
	}	
	
	/* try a basic message */

    msgid = ks_rpcmessage_create_request(h, "wombat", &parms, &request); 
	if (msgid == 0) {
		printf("failed to create a wombat\n");
		return;
	}

	cJSON_AddStringToObject(parms, "hello", "cruel world");
	char* data = cJSON_PrintUnformatted(request);	
	
	printf("\ntest01 request: %d\n%s\n\n", msgid, data);

	/* process message */
	
	ks_size_t size = strlen(data);
	ks_status_t status = ks_rpcmessage_process_message(h, (uint8_t*)data, size, &response);

	char* data1 = cJSON_Print(response);
	ks_size_t size1 = strlen(data1);
    printf("\ntest01i response: %d\n%s\n\n", msgid, data1);
	
	/* process response */

	ks_status_t status1 = ks_rpcmessage_process_message(h, (uint8_t*)data1, size1, &response);

	free(data);
	free(data1);
	cJSON_Delete(request);

	/* create message 2 */
	
	cJSON *parms1 = cJSON_CreateNumber(1);
    cJSON *request1  = NULL;

    msgid = ks_rpcmessage_create_request(h, "badbunny", &parms1, &request1);

	data = cJSON_PrintUnformatted(request1);
	printf("\ntest01i request: %d\n%s\n\n", msgid, data);

	/* process message 2 */

	size = strlen(data);
	status = ks_rpcmessage_process_message(h, (uint8_t*)data, size, &response);
 
	data1 = cJSON_PrintUnformatted(response);
    printf("\ntest01 response: %d\n%s\n\n", msgid, data1);
 
	/* process response 2 - no handler so this should fail */

    size1 = strlen(data1);
		
	status = ks_rpcmessage_process_message(h, (uint8_t*)data1, size1, &response);
	
	if (status != KS_STATUS_FAIL) {
		printf("badbunny found a response handler ?\n");
	}

    free(data);
    free(data1);
    cJSON_Delete(request1);
	

	printf("**** testrpcmessages - test01 complete\n\n\n"); fflush(stdout);
}

void test02()
{
	printf("**** testmessages - test02 start\n"); fflush(stdout);

	printf("****  testmessages - test02 finished\n"); fflush(stdout);

	return;
}




/* test06  */
/* ------  */

static void *testnodelocking_ex1(ks_thread_t *thread, void *data)
{
	return NULL;
}

static void *testnodelocking_ex2(ks_thread_t *thread, void *data)
{
	return NULL;
}


void test06()
{
	printf("**** testmessages - test06 start\n"); fflush(stdout);

	ks_thread_t *t0;
	ks_thread_create(&t0, testnodelocking_ex1, NULL, pool);

	ks_thread_t *t1;
	ks_thread_create(&t1, testnodelocking_ex2, NULL, pool);

	ks_thread_join(t1);
	ks_thread_join(t0);

	printf("\n\n* **testmessages - test06 -- threads complete\n\n"); fflush(stdout);

	printf("**** testmessages - test06 start\n"); fflush(stdout);

	return;
}



int main(int argc, char *argv[]) {

	printf("testmessages - start\n");

	int tests[100];
	if (argc == 0) {
		tests[0] = 1;
		tests[1] = 2;
		tests[2] = 3;
		tests[3] = 4;
		tests[4] = 5;
	}
	else {
		for(int tix=1; tix<100 && tix<argc; ++tix) {
			long i = strtol(argv[tix], NULL, 0);
			tests[tix] = i;
		}
	}

	ks_init();
	ks_global_set_default_logger(7);


	ks_status_t status;

	status = ks_pool_open(&pool);


	for (int tix=0; tix<argc; ++tix) {

		if (tests[tix] == 1) {
			ks_rpcmessage_init(pool, &h);
			test01();
			ks_rpcmessage_deinit(&h);
			continue;
		}

		if (tests[tix] == 2) {
			ks_rpcmessage_init(pool, &h);
			test02();
			ks_rpcmessage_deinit(&h);
			continue;
		}

	}

	return 0;
}
