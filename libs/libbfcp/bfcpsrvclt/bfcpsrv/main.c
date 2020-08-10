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
void print_requests_list(bfcp server, int index, int status);


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

	if (arguments->primitive <= 13) {
		printf("(%lu/%d/%d) %s %s\n", conferenceID, transactionID, userID, outgoing ? "--->" :"<---", bfcp_primitive[arguments->primitive-1].description);
	} else {
		printf("(%lu/%d/%d) %s %s\n", conferenceID, transactionID, userID, outgoing ? "--->" :"<---", "UNKNOWN PRIMITIVE");
	}

	return 0;
}


/* Menu to manipulate the Floor Control Server options and operations */
void menu(char *lineptr)
{
	char line[80];
	unsigned long int conferenceID = 0;
	bfcp list = NULL, server = NULL;
	int transport = 0, error = 0, max_num = 0, port = 0, Max_Num_Floors = 0,
		Max_Number_Floor_Request = 0, chair_automatic_accepted_policy = 0,
		chair_wait_request = 0, status = 0, limit_granted_floor = 0, i = 0, j = 0;
	unsigned int floorID = 0, chairID = 0, userID = 0;
	bfcp_conference *lconferences = NULL;
	bfcp_list_floors *list_floor = NULL;
	bfcp_list_users *list_user = NULL;
	bfcp_user *user = NULL;
	floor_query *query = NULL;
	char *text = NULL, *text1 = NULL;

	list = NULL;

	printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		"\n--------CONFERENCE SERVER-----------------------------------\n",
		" ?      - Show the menu\n",
		" c      - Create the FCS\n",
		" y      - Destroy the FCS\n",
		" i      - Insert a new conference\n",
		" d      - Delete a conference\n",
		" a      - Add a new floor\n",
		" f      - Delete a floor\n",
		" r      - Add a floor chair\n",
		" g      - Delete a floor chair\n",
		" j      - Add a new user\n",
		" k      - Delete a user\n",
		" h      - Change the max number of conferences\n",
                " w      - Change number of users that can be granted the same floor at the same time\n"
		" b      - Change the chair policy\n",
		" u      - Change maximum number of requests a user can make for the same floor\n",
		" s      - Show the conferences in the BFCP server\n",
		" q      - Quit\n",
		"------------------------------------------------------------------\n\n");
	while(fgets(line, 79, stdin) != NULL) {
		lineptr = line;
		while(*lineptr == ' ' || *lineptr == '\t')
			++lineptr;
		switch(*lineptr) {
			case '?':
				printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
					"\n--------CONFERENCE SERVER-----------------------------------\n",
					" ?      - Show the menu\n",
					" c      - Create the FCS\n",
					" y      - Destroy the FCS\n",
					" i      - Insert a new conference\n",
					" d      - Delete a conference\n",
					" a      - Add a new floor\n",
					" f      - Delete a floor\n",
					" r      - Add a floor chair\n",
					" g      - Delete a floor chair\n",
					" j      - Add a new user\n",
					" k      - Delete a user\n",
					" h      - Change the max number of conferences\n",
					" w      - Change number of users that can be granted the same floor at the same time\n"
					" b      - Change the chair policy\n",
					" u      - Change maximum number of requests a user can make for the same floor\n",
					" s      - Show the conferences in the BFCP server\n",
					" q      - Quit\n",
					"--------------------------------------------------------------\n\n");
				break;
			case 'c':
				++lineptr;
				printf("Enter the maximum number of allowed concurrent conferences in the system:\n");
				error = scanf("%i", &max_num);
				BFCP_CHECK_INTEGER;
				printf("Enter the listening port for the FCS:\n");
				error = scanf("%i", &port);
				BFCP_CHECK_INTEGER;
				printf("Enter the desired transport for the BFCP messages\n\t(0 for TCP/BFCP, 1 for TCP/TLS/BFCP, 2 for UDP/BFCP):\n");
				error = scanf ("%i", &transport);
				BFCP_CHECK_INTEGER;
				list = bfcp_initialize_bfcp_server(max_num, port, received_msg, transport, "server.pem", "server.key", "0.0.0.0");
				if(list == NULL)
					printf("Couldn't create the FCS (is the port already taken?)...\n");
				else
					printf("FCS created.\n");
				break;
			case 'i':
				++lineptr;
				printf("Enter the desired ConferenceID for the conference:\n");
				error = scanf("%li", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the maximum number of floors you want in this conference:\n");
				error = scanf("%i", &Max_Num_Floors);
				BFCP_CHECK_INTEGER;
				printf("Enter the maximum number of requests a user can make for the same floor:\n");
				error = scanf("%i", &Max_Number_Floor_Request);
				BFCP_CHECK_INTEGER;
				printf("Automated policy when chair is missing:\n\t0 = accept the request / 1 = don't\n");
				error = scanf("%u", &chair_automatic_accepted_policy);
				BFCP_CHECK_INTEGER;
				printf("Time in seconds the system will wait for a ChairAction: (0 for default number)\n");
				error = scanf("%i", &chair_wait_request);
				BFCP_CHECK_INTEGER;

				if(bfcp_initialize_conference_server(list, conferenceID, Max_Num_Floors, Max_Number_Floor_Request, chair_automatic_accepted_policy, chair_wait_request) < 0)
					printf("Couldn't add the conference %lu to the FCS...\n", conferenceID);
				else
					printf("Conference added.\n");
				break;
			case 'd':
				++lineptr;
				printf("Enter the conference you want to remove:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				if(bfcp_destroy_conference_server(list, conferenceID) < 0)
					printf("Couldn't remove the conference %lu from the FCS...\n", conferenceID);
				else
					printf("Conference removed.\n");
				break;
			case 'h':
				++lineptr;
				printf("Enter the maximum allowed number of concurrent conferences you want to have:\n");
				error = scanf("%i", &max_num);
				BFCP_CHECK_INTEGER;
				if(bfcp_change_number_bfcp_conferences_server(list, max_num) < 0)
					printf("Couldn't change the maximum allowed number of concurrent conferences...\n");
				else
					printf("Setting changed.\n");
				break;
			case 'u':
				++lineptr;
				printf("Enter the maximum number of requests a user can make for the same floor:\n");
				error = scanf("%i", &max_num);
				BFCP_CHECK_INTEGER;
				if(bfcp_change_user_req_floors_server(list, max_num) < 0)
					printf("Couldn't the maximum number of requests a user can make for the same floor:\n");
				else
					printf("Setting changed.\n");
				break;
			case 'y':
				++lineptr;
				error = bfcp_destroy_bfcp_server(&list);
				if(error < 0)
					printf("Couldn't destroy the FCS...\n");
				else
					printf("FCS destroyed.\n");
				break;
			case 's':
				++lineptr;
				server = list;
				if(server == NULL) {
					printf("The Floor Control Server is not up\n");
					break;
				}
				lconferences = server->list_conferences;
				if(lconferences == NULL) {
					printf("There are no conferences in the FCS\n");
					break;
				}

				if(server->Actual_number_conference == 0) {
					printf("There are no conferences in the FCS\n");
					break;
				}

				for(i = 0; i < server->Actual_number_conference; i++) {
					printf("CONFERENCE:\n");
					printf("ConferenceID: %lu\n", server->list_conferences[i].conferenceID);
					printf("\n");
					/* Print the list of floors */
					list_floor = server->list_conferences[i].floor;
					if(list_floor != NULL) {
						printf("Maximum number of floors in the conference: %i\n", list_floor->number_floors + 1);
						printf("FLOORS\n");
						for(j = 0; j < list_floor->actual_number_floors; j++) {
							printf("FloorID: %u, ", list_floor->floors[j].floorID);
							printf("ChairID: %u", list_floor->floors[j].chairID);
							if(list_floor->floors[j].floorState == BFCP_FLOOR_STATE_WAITING)
								printf(" state: FREE\n");
							else if(list_floor->floors[j].floorState == BFCP_FLOOR_STATE_ACCEPTED)
								printf(" state: ACCEPTED\n");
							else if(list_floor->floors[j].floorState >= BFCP_FLOOR_STATE_GRANTED)
								printf(" state: GRANTED\n");
							else
								printf(" state: error!\n");
							printf("Number of simultaneous granted users:% i\n", list_floor->floors[j].limit_granted_floor-1);
							query = list_floor->floors[j].floorquery;
							if(query != NULL)
								printf("QUERY LIST\n");
							while(query) {
								printf("User: %hu\n",query->userID);
								query = query->next;
							}
						}
					}
					/* Print the list of users */
					list_user = server->list_conferences[i].user;
					if(list_user != NULL) {
						user = list_user->users;
						printf("Maximum number of request per floors in the conference: %i\n", list_user->max_number_floor_request);
						printf("USERS\n");
						while(user) {
							printf("UserID: %hu\n", user->userID);
							user = user->next;
						}
					}
					/* Print the list of Pending requests */
					print_requests_list(server, i, BFCP_PENDING);
					/* Print the list of Accepted requests */
					print_requests_list(server, i, BFCP_ACCEPTED);
					/* Print the list of Granted requests */
					print_requests_list(server, i, BFCP_GRANTED);
				}
				printf("\n");
				break;
			case 'a':
				++lineptr;
				printf("Enter the conference you want to add a new floor to:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the desired FloorID:\n");
				error = scanf("%u", &floorID);
				BFCP_CHECK_INTEGER;
				printf("If a chair will manage this floor, enter its UserID\n\t(ChairID, 0 if no chair):\n");
				error = scanf("%u", &chairID);
				BFCP_CHECK_INTEGER;
				printf("Enter the maximum number of users that can be granted this floor at the same time\n\t(0 for unlimited users):\n");
				error = scanf("%u", &limit_granted_floor);
				BFCP_CHECK_INTEGER;
				if(bfcp_add_floor_server(list, conferenceID, floorID, chairID, limit_granted_floor) < 0)
					printf("Coudln't add the new floor...\n");
				else
					printf("Floor added.\n");
				break;
			case 'f':
				++lineptr;
				printf("Enter the conference you want to remove the floor from:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the FloorID you want to remove:\n");
				error = scanf("%u", &floorID);
				BFCP_CHECK_INTEGER;
				if(bfcp_delete_floor_server(list, conferenceID, floorID) < 0)
					printf("Coudln't remove the floor...\n");
				else
					printf("Floor removed.\n");
				break;
			case 'r':
				++lineptr;
				printf("Enter the conference you want to add a new chair to:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the FloorID this new chair will manage:\n");
				error = scanf("%u", &floorID);
				BFCP_CHECK_INTEGER;
				printf("Enter the UserID of the chair (ChairID):\n");
				error = scanf("%u", &chairID);
				BFCP_CHECK_INTEGER;
				error = bfcp_add_chair_server(list, conferenceID, floorID, chairID);
				if(error == 0)
					printf("Chair successfully added\n");
				else if(error == -1)
					printf("Couldn't setup the new chair...\n");
				break;
			case 'g':
				++lineptr;
				printf("Enter the conference you want to remove a chair from:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the FloorID the chair is currently handling:\n");
				error = scanf("%u", &floorID);
				BFCP_CHECK_INTEGER;
				error = bfcp_delete_chair_server(list, conferenceID, floorID);
				if(error == 0)
					printf("Chair successfully removed\n");
				else if(error == -1)
					printf("Couldn't remove the chair...\n");
				break;
			case 'j':
				++lineptr;
				printf("Enter the conference you want to add a new user to:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the UserID:\n");
				error = scanf("%u", &userID);
				BFCP_CHECK_INTEGER;
#ifndef WIN32
				printf("Enter the user's URI (e.g. bob@example.com):\n");
				scanf(" %a[^\n]", &text);
				printf("Enter the user's display name (e.g. Bob Hoskins):\n");
				scanf(" %a[^\n]", &text1);
#else
				/* FIXME fix broken scanf in WIN32 */
				text = calloc(10, sizeof(char));
				sprintf(text, "User %u", userID);
				text1 = calloc(20, sizeof(char));
				sprintf(text1, "user%u@localhost", userID);
				printf("Defaulting URI and Display to '%s':'%s'...\n", text, text1);
#endif
				if(bfcp_add_user_server(list, conferenceID, userID, text, text1) < 0)
					printf("Couldn't add the new user...\n");
				else
					printf("User added.\n");
				free(text);
				free(text1);
				break;
			case 'k':
				++lineptr;
				printf("Enter the conference you want to remove the user from:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the UserID:\n");
				error = scanf("%u", &userID);
				BFCP_CHECK_INTEGER;
				if(bfcp_delete_user_server(list, conferenceID, userID) < 0)
					printf("Couldn't remove the user...\n");
				else
					printf("User removed.\n");
				break;
			case 'w':
				++lineptr;
				printf("Enter the conference for which you want to change the maximum number of users that can be granted the same floor at the same time:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the FloorID:\n");
				error = scanf("%u", &floorID);
				BFCP_CHECK_INTEGER;
				printf("Enter the maximum number of users that can be granted this floor at the same time:\n");
				error = scanf("%u", &limit_granted_floor);
				BFCP_CHECK_INTEGER;
				if(bfcp_change_number_granted_floor_server(list, conferenceID, floorID, limit_granted_floor) < 0)
					printf("Couldn't change the settings for this conference...\n");
				else
					printf("Setting changed.\n");
				break;
			case 'b':
				++lineptr;
				printf("Enter conference for which you want to change the automated accept policy:\n");
				error = scanf("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Automated policy when chair is missing:\n\t0 = accept the request / 1 = don't\n");
				error = scanf("%u", &chair_automatic_accepted_policy);
				BFCP_CHECK_INTEGER;
				printf("Time in seconds the system will wait for a ChairAction: (0 for default number)\n");
				error = scanf("%i", &chair_wait_request);
				BFCP_CHECK_INTEGER;
				if(bfcp_change_chair_policy(list, conferenceID, chair_automatic_accepted_policy, chair_wait_request) < 0)
					printf("Couldn't change the settings for this conference...\n");
				else
					printf("Setting changed.\n");
				break;
			case 'q':
				status = 0;
				return;
				break;
			case '\n':
				break;
			default:
				printf("Invalid menu choice - try again\n");
				break;
		}
	}
}


/* Helper method to print the list of requests */
void print_requests_list(bfcp server, int index, int status)
{
	if(!server)
		return;

	bfcp_queue *list = NULL;
	bfcp_node *node = NULL;
	floor_request_query *query_floorrequest = NULL;

	switch(status) {
		case BFCP_PENDING:
			list = server->list_conferences[index].pending;
			break;
		case BFCP_ACCEPTED:
			list = server->list_conferences[index].accepted;
			break;
		case BFCP_GRANTED:
			list = server->list_conferences[index].granted;
			break;
		default:
			break;
	}

	bfcp_floor *FloorID = NULL;

	if(list != NULL) {
		node = list->head;
		if(node)
			printf("%s LIST\n", bfcp_status[status-1].description);
		while(node != NULL) {
			printf("FloorRequestID: %hu\n\tUserID: %hu Priority: %i Queue_position: %hu ", node->floorRequestID, node->userID, node->priority, node->queue_position);
			if(node->beneficiaryID != 0)
				printf("BeneficiaryID: %hu", node->beneficiaryID);
			printf("\n");
			FloorID = node->floor;
			while(FloorID) {
				printf("FloorID: %hu ", FloorID->floorID);
				if(FloorID->chair_info != NULL)
					printf("Chair-provided information: %s ", FloorID->chair_info);
				if(FloorID->status == BFCP_FLOOR_STATE_WAITING)
					printf(" state: FREE\n");
				else if(FloorID->status == BFCP_FLOOR_STATE_ACCEPTED)
					printf(" state: ACCEPTED\n");
				else if(FloorID->status >= BFCP_FLOOR_STATE_GRANTED)
					printf(" state: GRANTED\n");
				else
					printf(" state: error!\n");
				FloorID = FloorID->next;
			}
			query_floorrequest = node->floorrequest;
			while(query_floorrequest) {
				printf("FloorRequest query: %hu\n", query_floorrequest->userID);
				query_floorrequest = query_floorrequest->next;
			}
			node = node->next;
			printf("-----------------\n");
		}
	}
}


/* Main */
int main (int argc, char *argv[])
{
	char *lineptr = NULL;
	printf("\nBinary Floor Control Protocol (BFCP) Floor Control Server (FCS) Tester\n\n");
	menu(lineptr);

	return 0;
}
