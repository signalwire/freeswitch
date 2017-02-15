#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"

#include "../src/include/ks_rpcmessage.h"

#pragma GCC optimize ("O0")


ks_pool_t *pool;


void test01()
{
	printf("**** testrpcmessages - test01 start\n\n"); fflush(stdout);

	cJSON* request1  = NULL;
	cJSON* parms1    = NULL;
    cJSON* response1 = NULL;

														 /*namespace, method,  params, **request */	
	ks_rpcmessageid_t msgid = ks_rpcmessage_create_request("app1",     "func1", &parms1, &request1);
	if (msgid == 0) {
		printf("message create failed %d\n", msgid);
	}	
	
	cJSON_AddStringToObject(parms1, "hello", "cruel world");
	char* data = cJSON_PrintUnformatted(request1);	
	
	printf("test01 request1: %d\n%s\n\n", msgid, data);
    ks_json_pool_free(data);


	/* convert to buffer */
	cJSON* parms2  = NULL;
	ks_buffer_t *buffer;

	ks_buffer_create(&buffer, 256, 256, 1024);

    ks_size_t n = ks_rpc_create_buffer("app2", "func2", &parms2, buffer);

	ks_size_t size =  ks_buffer_len(buffer);
	char *b = (char *)ks_pool_alloc(pool, size+1);
	ks_buffer_read(buffer, b, size); 
	
	printf("test01 request2: %zd %zd  from ks_buffer\n%s\n\n\n", n, size, b); 
	

	/* create message 3 */
	
	cJSON *parms3 = cJSON_CreateNumber(1);
    cJSON *request3  = NULL;

    msgid = ks_rpcmessage_create_request("app1", "badbunny",  &parms3, &request3);
	data = cJSON_PrintUnformatted(request3);
	printf("\ntest01i request: %d\n%s\n\n", msgid, data);

	cJSON *response3 = NULL;

	ks_rpcmessage_create_response(request3,  NULL, &response3);  

	data = cJSON_PrintUnformatted(response3);
    printf("\ntest01 response3: %d\n%s\n\n", msgid, data);
 
    ks_json_pool_free(data);
    cJSON_Delete(request3);
    cJSON_Delete(response3);
	
	printf("**** testrpcmessages - test01 complete\n\n\n"); fflush(stdout);
}

void test02()
{
	printf("**** testmessages - test02 start\n"); fflush(stdout);

	printf("****  testmessages - test02 finished\n"); fflush(stdout);

	return;
}



int main(int argc, char *argv[]) {

	printf("testmessages - start\n");

	int tests[100];
	if (argc == 1) {
		tests[0] = 1;
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
			ks_rpcmessage_init(pool);
			test01();
			continue;
		}

		if (tests[tix] == 2) {
			ks_rpcmessage_init(pool);
			test02();
			continue;
		}

	}

	return 0;
}
