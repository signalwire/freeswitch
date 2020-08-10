#include "bfcp_participant.h"
#include <bfcp_strings.h>


/* Helper Macros to check for errors */
#define BFCP_CHECK_INTEGER				\
	if (error == 0) {				\
		printf("An integer is needed!\n");	\
		break;					\
	}

#define BFCP_CHECK_MESSAGE				\
	if(error == -1)					\
		printf("Error sending message...\n");	\
	else						\
		printf("Message sent!\n");


/* Headers */
void menu(char *lineptr);
void listen_server(bfcp_received_message *recv_msg);


/* Callback to receive notifications from the underlying library about incoming BFCP messages */
void listen_server(bfcp_received_message *recv_msg)
{
	int j, i;
	bfcp_supported_list *info_primitives, *info_attributes;
	bfcp_floor_request_information *tempInfo;
	bfcp_floor_request_status *tempID;

	if(!recv_msg) {
		printf("Invalid message received...\n");
		return;
	}
	if(!recv_msg->arguments) {
		printf("Invalid arguments in the received message...\n");
		bfcp_free_received_message(recv_msg);
		return;
	}
	if(!recv_msg->entity) {
		printf("Invalid IDs in the message header...\n");
		bfcp_free_received_message(recv_msg);
		return;
	}
	unsigned long int conferenceID = recv_msg->entity->conferenceID;
	unsigned short int userID = recv_msg->entity->userID;
	unsigned short int transactionID = recv_msg->entity->transactionID;

	/* Output will be different according to the BFCP primitive in the message header */
	switch(recv_msg->primitive) {
		case Error:
			printf("\nError:\n");
			if(!recv_msg->arguments->error)
				break;
			printf("\tError n.    %d\n", recv_msg->arguments->error->code);
			printf("\tError info: %s\n", recv_msg->arguments->eInfo ? recv_msg->arguments->eInfo : "No info");
			break;
		case HelloAck:
			printf("\nHelloAck:\n");
			info_primitives = recv_msg->arguments->primitives;
			printf("\tSupported Primitives:\n");
			while(info_primitives != NULL) {
				printf("\t\t%s\n", bfcp_primitive[info_primitives->element-1].description);
				info_primitives = info_primitives->next;
			}
			info_attributes=recv_msg->arguments->attributes;
			printf("\tSupported Attributes:\n");
			while(info_attributes != NULL) {
				printf("\t\t%s\n", bfcp_attribute[info_attributes->element-1].description);
				info_attributes = info_attributes->next;
			}
			printf("\n");
			break;
		case ChairActionAck:
			printf("ChairActionAck:\n");
			printf("\tTransactionID: %d\n", transactionID);
			printf("\tUserID         %d\n", userID);
			printf("\tConferenceID:  %lu\n", conferenceID);
			printf("\n");
			break;
		case FloorRequestStatus:
		case FloorStatus:
			if(recv_msg->primitive == FloorStatus)
				printf("FloorStatus:\n");
			else
				printf("FloorRequestStatus:\n");
			printf("\tTransactionID: %d\n", transactionID);
			printf("\tUserID         %d\n", userID);
			printf("\tConferenceID:  %lu\n", conferenceID);
			if(recv_msg->arguments->fID != NULL)
				printf("\tFloorID:       %d\n", recv_msg->arguments->fID->ID);
			if(recv_msg->arguments->frqInfo) {
				tempInfo = recv_msg->arguments->frqInfo;
				while(tempInfo) {
					printf("FLOOR-REQUEST-INFORMATION:\n");
					if(tempInfo->frqID)
						printf("   Floor Request ID:   %d\n", tempInfo->frqID);
					if(tempInfo->oRS) {
						printf("   OVERALL REQUEST STATUS:\n");
						if(tempInfo->oRS->rs){
							printf("      Queue Position  %d\n", tempInfo->oRS->rs->qp);
							printf("      RequestStatus   %s\n", bfcp_status[tempInfo->oRS->rs->rs-1].description);
						}
						if(tempInfo->oRS->sInfo)
						printf("      Status Info:   %s\n", tempInfo->oRS->sInfo);
					}
					if(tempInfo->fRS) {
						printf("   FLOOR REQUEST STATUS:\n");
						tempID = tempInfo->fRS;
						j = 0;
						while(tempID) {
							printf("   FLOOR IDs:\n");
							j++;
							printf("      (n.%d):  %d\n", j, tempID->fID);
							if(tempID->rs->rs)
								printf("      RequestStatus  %s\n", bfcp_status[tempID->rs->rs-1].description);
							printf("      Status Info:   %s\n", tempID->sInfo);
							tempID = tempID->next;
						}
					}
					if(tempInfo->beneficiary) {
						printf("   BENEFICIARY-INFORMATION:\n");
						if(tempInfo->beneficiary->ID)
							printf("      Benefeciary ID: %d\n", tempInfo->beneficiary->ID);
						if(tempInfo->beneficiary->display)
							printf("      Display Name:   %s\n", tempInfo->beneficiary->display);
						if(tempInfo->beneficiary->uri)
							printf("      User URI:       %s\n", tempInfo->beneficiary->uri);
					}
					if(tempInfo->requested_by) {
						printf("    REQUESTED BY INFORMATION:\n");
						if(tempInfo->requested_by->ID)
							printf("      Requested-by ID:  %d\n", tempInfo->requested_by->ID);
						if(tempInfo->requested_by->display)
							printf("      Display Name:     %s\n", tempInfo->requested_by->display);
						if(tempInfo->requested_by->uri)
							printf("      User URI:         %s\n", tempInfo->requested_by->uri);
					}
					if(tempInfo->priority)
						printf("    PRIORITY:                   %d\n", tempInfo->priority);
					if(tempInfo->pInfo)
						printf("    PARTICIPANT PROVIDED INFO:  %s\n", tempInfo->pInfo);
					printf("---\n");
					tempInfo=tempInfo->next;
				}
			}
			printf("\n");
			break;
		case UserStatus:
			printf("UserStatus:\n");
			printf("\tTransactionID: %d\n", transactionID);
			printf("\tUserID         %d\n", userID);
			printf("\tConferenceID:  %ld\n", conferenceID);
			i = 0;
			if(recv_msg->arguments->beneficiary)
				printf("BeneficiaryInformation %d:\n", recv_msg->arguments->beneficiary->ID);
			tempInfo=recv_msg->arguments->frqInfo;
			while(tempInfo) {
				i++;
				printf("FLOOR-REQUEST-INFORMATION (%d):\n",i);
				tempID = tempInfo->fRS;
				j = 0;
				while(tempID) {
					printf("   FLOOR IDs:\n");
					j++;
					printf("      (n.%d): %d\n", j, tempID->fID);
					if(tempID->rs->rs) printf("      RequestStatus  %s\n", bfcp_status[tempID->rs->rs-1].description);
					printf("      Status Info:   %s\n", tempID->sInfo ? tempID->sInfo : "No info");
					tempID = tempID->next;
				}
				printf("   FloorRequestID %d\n", tempInfo->frqID);
				if(tempInfo->oRS) {
					printf("   OVERALL REQUEST STATUS:\n");
					if(tempInfo->oRS->rs){
						printf("      Queue Position  %d\n", tempInfo->oRS->rs->qp);
						printf("      RequestStatus   %s\n", bfcp_status[tempInfo->oRS->rs->rs-1].description);
					}
					if(tempInfo->oRS->sInfo)
					printf("      Status Info:   %s\n", tempInfo->oRS->sInfo ? tempInfo->oRS->sInfo : "No info");
				}
				if(tempInfo->beneficiary)
					printf("   BeneficiaryID  %d\n", tempInfo->beneficiary->ID);
				if(tempInfo->requested_by)
					printf("   Requested_byID %d\n", tempInfo->requested_by->ID);
				printf("   Participant Provided info:     %s\n", tempInfo->pInfo ? tempInfo->pInfo : "No info");
				tempInfo = tempInfo->next;
			}
			printf("\n");
			break;
		default:
			break;
	}

	if(recv_msg != NULL)
		bfcp_free_received_message(recv_msg);
}

/* Menu to manipulate the Floor Control Server options and operations */
void menu(char *lineptr)
{
	char line[80], *text = NULL, *text1 = NULL;
	unsigned long int conferenceID = 0;
	unsigned int userID = 0, floorID = 0, beneficiaryID = 0;
	bfcp_participant_information *list = NULL;
	bfcp_floors_participant *node = NULL, *list_floor = NULL;
	floors_participant temp_floors = NULL;
	int transport = -1, error = 0, status = 0, port_server = 0, priority = 0, queue_position = 0;
	unsigned short int floorRequestID = 0;

	list = NULL;

	printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		"\n--------PARTICIPANT LIST-----------------------------\n",
		" ?      - Show the menu\n",
		" c      - Create the Participant\n",
		" h      - Destroy the Participant\n",
		" i      - Insert a new floor\n",
		" d      - Remove a floor\n",
		" s      - Show the information of the conference\n",
		"BFCP Messages:\n",
		" f      - Hello\n",
		" r      - FloorRequest\n",
		" l      - FloorRelease\n",
		" o      - FloorRequestQuery\n",
		" u      - UserQuery\n",
		" e      - FloorQuery\n",
		" a      - ChairAction\n",
		" q      - quit                   \n",
		"------------------------------------------------------\n\n");
	while(fgets(line, 79, stdin) != NULL) {
		lineptr = line;
		while(*lineptr == ' ' || *lineptr == '\t')
			++lineptr;
		switch(*lineptr) {
			case '?':
				printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
					"\n--------PARTICIPANT LIST-----------------------------\n",
					" ?      - Show the menu\n",
					" c      - Create the Participant\n",
					" h      - Destroy the Participant\n",
					" i      - Insert a new floor\n",
					" d      - Remove a floor\n",
					" s      - Show all the participant information\n",
					"BFCP Messages:\n",
					" r      - FloorRequest message\n",
					" l      - FloorRelease message\n",
					" o      - FloorRequestQuery message\n",
					" u      - UserQuery message\n",
					" e      - FloorQuery message\n",
					" a      - ChairAction message\n",
					" f      - Hello message\n",
					" q      - quit                   \n",
					"------------------------------------------------------\n\n");
				break;
			case 'c':
				++lineptr;
				printf("Enter the conferenceID of the conference:\n");
				error = scanf ("%lu", &conferenceID);
				BFCP_CHECK_INTEGER;
				printf("Enter the userID of the participant:\n");
				error = scanf ("%i", &userID);
				BFCP_CHECK_INTEGER;
				printf("Enter the IP address of the Floor Control Server: (e.g. 127.0.0.1)\n");
				text1 = calloc(20, sizeof(char));
				scanf ("%s", text1);
				printf(" --> %s\n", text1 ? text1 : "??");
				printf("Enter the listening port of the Floor Control Server: (e.g. 2345)\n");
				error = scanf ("%i", &port_server);
				BFCP_CHECK_INTEGER;
				printf("Enter the desired transport for the BFCP messages\n\t(0 for TCP/BFCP, 1 for TCP/TLS/BFCP):\n");
				error = scanf ("%i", &transport);
				BFCP_CHECK_INTEGER;
				list = bfcp_initialize_bfcp_participant(conferenceID, userID, text1, port_server, listen_server, transport);
				if(list == NULL)
					printf("Couldn't create the new BFCP participant...\n");
				else
					printf("BFCP Participant created.\n");
				free(text1);
				break;
			case 'i':
				++lineptr;
				printf("Enter the FloorID:\n");
				error = scanf ("%i", &floorID);
				BFCP_CHECK_INTEGER;
				if(bfcp_insert_floor_participant(list, floorID) < 0)
					printf("Couldn't add the new floor...\n");
				else
					printf("Floor added.\n");
				break;
			case 'd':
				++lineptr;
				printf("Enter the FloorID:\n");
				error = scanf ("%u", &floorID);
				BFCP_CHECK_INTEGER;
				if(bfcp_delete_floor_participant(list, floorID) < 0)
					printf("Couldn't remove the floor...\n");
				else
					printf("Floor removed.\n");
				break;
			case 'h':
				++lineptr;
				if(bfcp_destroy_bfcp_participant(&list) < 0)
					printf("Couldn't destroy the BFCP participant...\n");
				else
					printf("BFCP participant destroyed.\n");
				break;
			case 'r':
				++lineptr;
				printf("Enter the BeneficiaryID of the new request (0 if it's not needed):\n");
				error = scanf ("%u", &beneficiaryID);
				BFCP_CHECK_INTEGER;
				printf("Enter the priority for this request (0=lowest --> 4=highest):\n");
				error = scanf ("%i", &priority);
				BFCP_CHECK_INTEGER;
#ifndef WIN32
				printf("Enter some participant-provided information, if needed:\n");
				scanf (" %a[^\n]", &text);
#else
				/* FIXME fix broken scanf in WIN32 */
				text = calloc(20, sizeof(char));
				sprintf(text, "Let me talk!");
#endif
				printf("Enter the FloorID:\n");
				error = scanf ("%u", &floorID);
				BFCP_CHECK_INTEGER;
				node = create_floor_list_p(floorID, NULL);
				printf("Enter another FloorID: (0 to stop inserting floors)\n");
				error = scanf ("%u", &floorID);
				BFCP_CHECK_INTEGER;
				while (floorID != 0) {
					node = insert_floor_list_p(node, floorID, NULL);
					printf("Enter another FloorID: (0 to stop inserting floors)\n");
					error = scanf ("%u", &floorID);
					BFCP_CHECK_INTEGER;
				}
				error = bfcp_floorRequest_participant(list, beneficiaryID, priority, node, text);
				if((error == -1) && (node != NULL)) {
					/* Free the request */
					printf("Error sending the message: do the floors in the request exist?\n");
					remove_floor_list_p(node);
				} else
					printf("Message sent!\n");
				free(text);
				break;
			case 'l':
				++lineptr;
				printf("Enter the FloorRequestID of the request you want to release:\n");
				error = scanf ("%hu", &floorRequestID);
				BFCP_CHECK_INTEGER;
				error = bfcp_floorRelease_participant(list, floorRequestID);
				BFCP_CHECK_MESSAGE;
				break;
			case 'o':
				++lineptr;
				printf("Enter the FloorRequestID of the request you're interested in:\n");
				error = scanf ("%hu", &floorRequestID);
				BFCP_CHECK_INTEGER;
				error = bfcp_floorRequestQuery_participant(list, floorRequestID);
				BFCP_CHECK_MESSAGE;
				break;
			case 'u':
				++lineptr;
				printf("Enter the BeneficiaryID if you want information about a specific user (0 instead):\n");
				error = scanf ("%u", &beneficiaryID);
				BFCP_CHECK_INTEGER;
				error = bfcp_userQuery_participant(list, beneficiaryID);
				BFCP_CHECK_MESSAGE;
				break;
			case 'e':
				++lineptr;
				printf("Enter the FloorID  of the floor you're interested in: (0 to stop inserting floors):\n");
				error = scanf ("%u", &floorID);
				BFCP_CHECK_INTEGER;
				if(floorID == 0)
					node = NULL;
				else {
					node = create_floor_list_p(floorID, NULL);
					printf("Enter the FloorID  of the floor you're interested in: (0 to stop inserting floors)\n");
					error = scanf ("%u", &floorID);
					BFCP_CHECK_INTEGER;
					while (floorID != 0){
						node = insert_floor_list_p(node, floorID, NULL);
						printf("Enter the FloorID  of the floor you're interested in: (0 to stop inserting floors)\n");
						error = scanf ("%u", &floorID);
						BFCP_CHECK_INTEGER;
					}
				}
				error = bfcp_floorQuery_participant(list, node);
				if(error == -1)
					remove_floor_list_p(node);
				BFCP_CHECK_MESSAGE;
				break;
			case 'a':
				++lineptr;
				printf("Enter the FloorRequestID:\n");
				error = scanf ("%hu", &floorRequestID);
				BFCP_CHECK_INTEGER;
				printf("What to do with this request? (2=Accept / 4=Deny / 7=Revoke):\n");
				error = scanf ("%u", &status);
				BFCP_CHECK_INTEGER;
				printf("Enter the FloorID to act upon:\n");
				error = scanf ("%u", &floorID);
				BFCP_CHECK_INTEGER;
#ifndef WIN32
				printf("Enter explanatory text about your decision concerning this floor:\n");
				scanf (" %a[^\n]", &text);
#else
				/* FIXME fix broken scanf in WIN32 */
				text = calloc(20, sizeof(char));
				sprintf(text, "Nothing personal");
#endif
				list_floor = create_floor_list_p(floorID, text);
				free(text);

				printf("Enter another FloorID: (0 to stop inserting floors)\n");
				error = scanf ("%u", &floorID);
				BFCP_CHECK_INTEGER;
				if(floorID != 0) {
#ifndef WIN32
					printf("Enter explanatory text about your decision concerning this floor:\n");
					scanf (" %a[^\n]", &text);
#else
					/* FIXME fix broken scanf in WIN32 */
					text = calloc(20, sizeof(char));
					sprintf(text, "Nothing personal");
#endif
				}
				while(floorID != 0) {
					list_floor = insert_floor_list_p(list_floor, floorID, text);
					free(text);
					printf("Enter another FloorID: (0 to stop inserting floors)\n");
					error = scanf ("%u", &floorID);
					BFCP_CHECK_INTEGER;
					if(floorID != 0) {
#ifndef WIN32
						printf("Enter explanatory text about your decision concerning this floor:\n");
						scanf (" %a[^\n]", &text);
#else
						/* FIXME fix broken scanf in WIN32 */
						text = calloc(20, sizeof(char));
						sprintf(text, "Nothing personal");
#endif
					}
				}
#ifndef WIN32
				printf("Enter explanatory text about your decision:\n");
				scanf (" %a[^\n]", &text);
#else
				/* FIXME fix broken scanf in WIN32 */
				text = calloc(20, sizeof(char));
				sprintf(text, "Accept my decision");
#endif
				printf("Enter the desired position in queue for this request (0 if you are not interested in that):\n");
				error = scanf ("%u", &queue_position);
				BFCP_CHECK_INTEGER;
				error = bfcp_chairAction_participant(list, floorRequestID, text, status, list_floor, queue_position);
				free(text);
				if(error == -1)
					remove_floor_list_p(node);
				BFCP_CHECK_MESSAGE;
				break;
			case 'f':
				++lineptr;
				error = bfcp_hello_participant(list);
				BFCP_CHECK_MESSAGE;
				break;
			case 's':
				++lineptr;
				if(list == NULL)
					break;
				printf("conferenceID: %lu\n", list->conferenceID);
				printf("userID: %u\n", list->userID);
				temp_floors= list->pfloors;
				while(temp_floors != NULL) {
					printf("FloorID: %u ", temp_floors->floorID);
					printf("\n");
					temp_floors = temp_floors->next;
				}
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

/* Main */
int main (int argc, char *argv[])
{
	char *lineptr = NULL;
	printf("\nBinary Floor Control Protocol (BFCP) Participant Tester\n\n");
	menu(lineptr);

	return 0;
}
