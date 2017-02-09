#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"


#include <blade_rpcproto.h>

#pragma GCC optimize ("O0")


ks_pool_t *pool;
ks_thread_pool_t *tpool;


static ks_thread_t *threads[10];

static char idbuffer[51];



static enum jrpc_status_t  process_widget(cJSON *msg, cJSON **response)
{
    printf("entering process_widget\n");

    char *b0 = cJSON_PrintUnformatted(msg);
    printf("Request: %s\n", b0);
    ks_pool_free(pool, &b0);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "code", 199);

    ks_rpcmessage_id msgid = ks_rpcmessage_create_response(msg, &resp, response);

    char *b1 = cJSON_PrintUnformatted(*response);   //(*response);
    printf("Response: msgid %d\n%s\n", msgid, b1);
    ks_pool_free(pool, &b1);

    printf("exiting process_wombat\n");

    return JRPC_SEND;
}


static enum jrpc_status_t  process_widget_response(cJSON *request, cJSON **msg)
{
	printf("entering process_widget_response\n");
	printf("exiting process_widget_response\n");
	return JRPC_PASS;
}



static enum jrpc_status_t  process_wombat(cJSON *msg, cJSON **replyP)
{
	printf("entering process_wombat\n");
	
	char *b0 = cJSON_PrintUnformatted(msg);
	printf("\nRequest: %s\n\n", b0);
	ks_pool_free(pool, &b0);

	cJSON *result = cJSON_CreateObject();
	cJSON_AddNumberToObject(result, "code", 99);
	cJSON *response;

    ks_rpcmessage_id msgid = ks_rpcmessage_create_response(msg, &result, &response);

	cJSON *response_copy = cJSON_Duplicate(response, 1);
    blade_rpc_process_jsonmessage(response_copy);

    if (msgid != 0) {
	    char *b1 = cJSON_PrintUnformatted(response);   //(*response);
	    printf("\nResponse: msgid %d\n%s\n\n", msgid, b1);
        blade_rpc_write_json(response);
		ks_pool_free(pool, &b1);
    }
    else {
        printf("process_wombat_preresponse: unable to create response \n");
        return JRPC_ERROR;
    }


	cJSON *parms2 = NULL;
	msgid = ks_rpcmessage_create_request("app1", "widget", "99", "1.0", &parms2, replyP);

    printf("\n\nexiting process_wombat with a reply to send\n");

	return JRPC_SEND; 
}

static enum jrpc_status_t  process_wombat_prerequest(cJSON *request, cJSON **msg)
{
    printf("entering process_wombat_prerequest\n");
    printf("exiting process_wombat_prerequest\n");
    return JRPC_SEND;
}

static enum jrpc_status_t  process_wombat_postrequest(cJSON *request, cJSON **msg)
{
    printf("entering process_wombat_postrequest\n");
    printf("exiting process_wombat_postrequest\n");
    return JRPC_PASS;
}



static enum jrpc_status_t  process_wombat_response(cJSON *request, cJSON **msg)
{
	printf("entering process_wombat_response\n");
	printf("exiting process_wombat_response\n");
	return JRPC_PASS;
}

static enum jrpc_status_t  process_wombat_preresponse(cJSON *request, cJSON **msg)
{

    printf("entering process_wombat_preresponse\n");

	cJSON *response = NULL;
	cJSON *result = NULL;

	cJSON *parms2 = NULL;

	//ks_rpcmessage_id msgid = ks_rpcmessage_create_request("app1", "widget", "99", "1.0", &parms2, msg);	

    printf("exiting process_wombat_preresponse\n");
    return JRPC_SEND;
}

static enum jrpc_status_t  process_wombat_postresponse(cJSON *request, cJSON **msg)
{
    printf("entering process_postwombat_response\n");
    printf("exiting process_postwombat_response\n");
    return JRPC_PASS;
}




static enum jrpc_status_t  process_badbunny( cJSON *msg, cJSON **response)
{
    printf("entering process_badbunny\n");

    char *b0 = cJSON_PrintUnformatted(msg);
    printf("\nRequest: %s\n\n", b0);
    ks_pool_free(pool, &b0);

    cJSON *respvalue;

    ks_rpcmessage_id msgid = ks_rpcmessage_create_errorresponse(msg, &respvalue, response);

    char *b2 = cJSON_PrintUnformatted(*response);
    printf("\nRequest: msgid %d\n%s\n\n", msgid, b2);
    ks_pool_free(pool, &b2);

	//cJSON *respvalue = cJSON_CreateNumber(1);
    	

    char *b1 = cJSON_PrintUnformatted(*response);   //(*response);
    printf("\nResponse: %s\n\n", b1);
    ks_pool_free(pool, &b1);

    printf("exiting process_badbunny\n");


    return JRPC_SEND;
}


void test01()
{
	printf("**** testrpcmessages - test01 start\n"); fflush(stdout);

	blade_rpc_declare_template("temp1", "1.0");

	blade_rpc_register_template_function("temp1", "widget", process_widget, process_widget_response); 

    blade_rpc_declare_namespace("app1", "1.0");

	blade_rpc_register_function("app1", "wombat", process_wombat, process_wombat_response); 

    blade_rpc_register_custom_request_function("app1", "wombat", process_wombat_prerequest, process_wombat_postresponse);
    blade_rpc_register_custom_response_function("app1", "wombat", process_wombat_preresponse, process_wombat_postresponse);


	/* message 1 */
	/* --------- */
    cJSON* request1 = NULL;
    cJSON* parms1   = NULL;

	printf("\n\n\n - message1 - basic message\n\n\n");

	ks_rpcmessage_id msgid = ks_rpcmessage_create_request("app1", "wombat", "99", "1.0", &parms1, &request1);
	if (msgid == 0) {
		printf("test01.1: unable to create message 1\n");
		return;
	}	

	if (!request1) {
		printf("test01.1: No json returned from create request 1\n");
		return;
	}

	char *pdata = cJSON_PrintUnformatted(request1);

	if (!pdata) {
		printf("test01.1: unable to parse cJSON object\n");
		return;
	}

	printf("request:\n%s\n", pdata);

    cJSON_AddStringToObject(parms1, "hello", "cruel world");

	blade_rpc_process_jsonmessage(request1);

	cJSON_Delete(request1);

	ks_pool_free(pool, &pdata);

	printf("\ntest01.1 complete\n");

	
	/* message 2 */
	/* --------- */

    printf("\n\n\n - message2 - test inherit\n\n\n");

	blade_rpc_inherit_template("app1", "temp1");

    cJSON* request2 = NULL;
    cJSON* parms2   = NULL;

    msgid = ks_rpcmessage_create_request("app1", "temp1.widget", "99", "1.0", &parms2, &request2); 
	if (msgid == 0) {
		printf("test01.2: failed to create a wombat\n");
		return;
	}

    if (!request2) {
        printf("test01.2: No json returned from create request 1\n");
        return;
    }

    pdata = cJSON_PrintUnformatted(request2);

	if (!pdata) {
		printf("test01.2: unable to parse cJSON object\n");
		return;
	}

    printf("request:\n%s\n", pdata);

    cJSON_AddStringToObject(parms2, "hello2", "cruel world2");

    blade_rpc_process_jsonmessage(request2);

    cJSON_Delete(request2);
    ks_pool_free(pool, &pdata);

    printf("\ntest01.2 complete\n");

	return;

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

	blade_rpc_init(pool);

	for (int tix=0; tix<argc; ++tix) {


		if (tests[tix] == 1) {
			test01();
			continue;
		}

		if (tests[tix] == 2) {
			test02();
			continue;
		}

	}

	return 0;
}
