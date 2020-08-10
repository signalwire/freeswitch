#include "bfcp_server.h"
#include <bfcp_strings.h>


/* Helper Macro to check for integer errors */
#define BFCP_CHECK_INTEGER				\
	if(error == 0) {				\
		printf("An integer is needed!\n");	\
		break;					\
	}


/* Headers */
void menu(char *lineptr);
int received_msg(bfcp_arguments *arguments, int outgoing);


/* Callback to receive notifications from the underlying library about incoming BFCP messages */
int received_msg(bfcp_arguments *arguments, int outgoing)
{
	if(!arguments) {
		printf("Invalid arguments in the received message...\n");
		return -1;
	}
	if(!arguments->entity) {
		printf("Invalid IDs in the message header...\n");
		bfcp_free_arguments(arguments);
		return -1;
	}
	unsigned long int conferenceID = arguments->entity->conferenceID;
	unsigned short int userID = arguments->entity->userID;
	unsigned short int transactionID = arguments->entity->transactionID;

	if(arguments != NULL)
		printf("(%lu/%d/%d) %s %s\n", conferenceID, transactionID, userID, outgoing ? "--->" :"<---", bfcp_primitive[arguments->primitive-1].description);

	return 0;
}


/* Menu to manipulate the Floor Control Server options and operations */
void menu(char *lineptr)
{
	unsigned long int conferenceID = 0;
	bfcp list = NULL;
	int i = 0;
	unsigned int userID = 0;
	char *text = NULL, *text1 = NULL;

	list = bfcp_initialize_bfcp_server(1, 2345, received_msg, 0, "server.pem", "server.key");
	if(list == NULL) {
		printf("Couldn't create the FCS (is the port already taken?)...\n");
		return;
	} else
		printf("FCS created.\n");

	conferenceID = 8771000;
	if(bfcp_initialize_conference_server(list, conferenceID, 2, 1, 0, 0) < 0) {
		printf("Couldn't add the conference %lu to the FCS...\n", conferenceID);
		return;
	} else
		printf("Conference added.\n");

	if(bfcp_add_floor_server(list, conferenceID, 11, 0, 0) < 0) {
		printf("Coudln't add the new floor...\n");
		return;
	} else
		printf("Floor added.\n");

	for(i=0; i < 190; i++) {
		userID = i+1;
		text = calloc(10, sizeof(char));
		sprintf(text, "User %u", userID);
		text1 = calloc(20, sizeof(char));
		sprintf(text1, "user%u@localhost", userID);
		if(bfcp_add_user_server(list, conferenceID, userID, text, text1) < 0)
			printf("Couldn't add the new user...\n");
		else
			printf("User %hu added.\n", userID);
		free(text);
		free(text1);
	}

	while(1)
		sleep(1);
}


/* Main */
int main (int argc, char *argv[])
{
	char *lineptr = NULL;
	printf("\nBinary Floor Control Protocol (BFCP) Floor Control Server (FCS) Tester\n\n");
	menu(lineptr);

	return 0;
}
