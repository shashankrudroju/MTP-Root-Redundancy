/*
 *	Main.c
 *
 *
 *  Created on: Sep 21, 2015
 *  Author: Pranav Sai(pk6420@rit.edu), Peter Willis (pjw7904@rit.edu)
 *
 * testing
 */
#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <signal.h>
#include <ctype.h>

#include "feature_payload.h"
#include "mtp_send.h"

//MTP Ethertype
#define ETH_MTP_CTRL    0x8850
#define ETH_UNI_DATA    0xff00

#define MAX_VID_LIST    20

//I belive "172" is a string in this case for comparsion later to check if its a given private address by GENI/control eth0 interface
#define CTRL_IP        "172"

struct timespec start, end;

//Function Prototypes
void mtp_start();

int getActiveInterfaces(char **);

void learn_active_interfaces();

bool checkInterfaceIsActive(char *);

//Global variables
// Shashank : Need two variables here. isPrimaryRoot and isSecondaryRoot
bool isPrimaryRoot = false;

bool isSecondaryRoot = false;

bool treeStable = false;

int  treetoUse = 1;
// Shashank : Need a root priority variable that will set this priority to 1 as long as this root is up
// Shashank : Once this root is down, we change its priority to 0 and make the secondary root's priority to 1

char *primaryRootVID = '\0';
char *secondaryRootVID = '\0';
struct interface_tracker_t *interface_head = NULL;

//main function, protocol begins here
int main(int argc, char **argv) {
    char **interfaceNames;

    /*Check number of Arguments, after the program name
     *A "1" argument means that the switch is the root of a meshed tree, a "0" means that it is a child
     *If a "1" is specified first, then another argument has to be added that tells the program what the root VID is
     *Ex: ./mtp 1 1 = I am the root switch, and my VID (the root VID) is 1
     */
    //Shashank : The arguments will change..based on whether its a Primary root or a secondary root
    //Shashank : If its a primary root, the arguments will be ./main 1 <VID> else if its secondary ./main 2 <VID>, if its a non root then ./main 0
    if (argc < 2) {
        printf("Error: Node spec or ROOT MTS ID missing. Format ./main <non MTS/root MTS> <ROOT MTS ID>\n");
        printf("Error: 0 for non MTS, 1 for root MTS\n");
        exit(1);
    }

    //Check if Node is Root MTS or Non MTS, atoi converts a string argument into an integer (why >= 1 and not == 1?)
    // Shashank : Add extra condition and check if its primary of secondary and update the appropriate isPrimaryRoot flag to true
    // Shashank : Also need to set the priority of primary root to 1
    if (atoi(argv[1]) == 1) {
        isPrimaryRoot = true;
        primaryRootVID = argv[2];
        //  Check if Root VID is provided through CLI.
        printf("This is the Primary root\n");
        if (primaryRootVID == NULL) {
            printf("Error: Provide ROOT Switch ID ./main <non MTS/root MTS> <ROOT MTS ID>\n");
            exit(1);
        }
    }
    if (atoi(argv[1]) == 2) {
        isSecondaryRoot = true;
        secondaryRootVID = argv[2];
        printf("This is the Secondary root\n");
        //  Check if Root VID is provided through CLI.
        if (secondaryRootVID == NULL) {
            printf("Error: Provide ROOT Switch ID ./main <non MTS/root MTS> <ROOT MTS ID>\n");
            exit(1);
        }
    }
    //printf("Check for type of node done\n");
    /*
     *Intially we mark all ports as host ports, if we get a MTP CTRL frame from any port we remove it. The local host broadcast table is populated as a result of the
     *ports all being host ports
     */
    interfaceNames = (char **) calloc(MAX_INTERFACES * MAX_INTERFACES, sizeof(char));
    memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
    //returns the number of interfaces
    int numberOfInterfaces = getActiveInterfaces(interfaceNames);

    int i = 0;
    for (; i < numberOfInterfaces; i++) {
        // Allocate memory and intialize(calloc), local_bcast_tuple has two vars: ethernet port name, and a pointer to the next local_bcast_tuple
        struct local_bcast_tuple *new_node = (struct local_bcast_tuple *) calloc(1, sizeof(struct local_bcast_tuple));

        // eth_name is the one var in local_bcast_tuple, which is the destination in the string copy
        strncpy(new_node->eth_name, interfaceNames[i], strlen(interfaceNames[i]));
        new_node->next = NULL;
        add_entry_lbcast_LL(new_node);
    }
    free(interfaceNames);

    /*

    The inital sending of ADVTs (root) and JOINs (non-root) has been moved down to the mtp_start() function

    */

    learn_active_interfaces();
    mtp_start();

    return 0;
}

/* Start MTP Protocol. */
void mtp_start() {
    int sockCtrl = 0, sockData = 0, recv_len = 0;
    uint8_t recvBuffer[MAX_BUFFER_SIZE] = {'\0'}; //init all values to the NULL byte
    struct ether_header *eheader = NULL;
    struct sockaddr_ll src_addr;
    char **interfaceNames, **deletedVIDs, **deletedVIDs2;
    // time_t, timers for checking hello time.
    time_t time_advt_beg;
    time_t time_advt_fin;
    time_t time_advt_beg2;
    time_t time_advt_fin2;


    // clear the memory
    interfaceNames = (char **) calloc(MAX_INTERFACES * MAX_INTERFACES, sizeof(char));
    deletedVIDs = (char **) calloc(MAX_VID_LIST * MAX_VID_LIST, sizeof(char));
    deletedVIDs2 = (char **) calloc(MAX_VID_LIST * MAX_VID_LIST, sizeof(char));

    // Create Socket, ETH_MTP_CTRL is used because we are listening packets of all kinds.
    if ((sockCtrl = socket(AF_PACKET, SOCK_RAW, htons(ETH_MTP_CTRL))) < 0) {
        perror("Error: MTP socket()");
        exit(1);
    }

    // Create Socket, ETH_ is used because we are listening packets of all kinds.
    if ((sockData = socket(AF_PACKET, SOCK_RAW, htons(ETH_UNI_DATA))) < 0) {
        perror("Error: MTP socket()");
        exit(1);
    }

    //timing unix commands
    system("echo START TIME: >> MSTC.txt");
    system("date +%H:%M:%S:%N >> MSTC.txt");


    //starts the clocks for periodic hello messages and MSTC timing
    time(&time_advt_beg);
    time(&time_advt_beg2);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    //----------------------------------------------------------------------------
    memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
    int numberOfInterfaces = getActiveInterfaces(interfaceNames);
    //printf("Before checking for root/non-root\n");
    // Shashank : Check if it is either primary root or secondary root and based on that create the node and add it to the appropriate linkedlist
    // If Node is Root MTS
    if (isPrimaryRoot) {
            struct vid_addr_tuple *new_node = (struct vid_addr_tuple *) calloc(1, sizeof(struct vid_addr_tuple));

            // Create VID structure for user-defined root VID
            strncpy(new_node->vid_addr, primaryRootVID, strlen(primaryRootVID));
            strcpy(new_node->eth_name,
                   "self");    // own interface, so mark it as self, will be helpful while tracking own VIDs.
            new_node->last_updated = -1;                // -1 here because root ID should not be removed.
            new_node->port_status = PVID_PORT;
            new_node->next = NULL;
            new_node->isNew = true;
            new_node->path_cost = PATH_COST;
            new_node->membership = 1;

            // Add into VID Table.
            system("echo This is the Primary root and adding it to the primary linked list >> MSTC.txt");
            printf("This is the Primary root and adding it to the primary linked list\n");
            add_entry_LL(new_node);

            int i = 0;
            uint8_t *payload = NULL;
            uint8_t payloadLen;
            int treeNo = 1;
            treeStable = true;

            for (; i < numberOfInterfaces; i++) {
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                // Shashank : When building the VID_ADVT payload send the appropriate VID linkedlist based on the treeNo
                //printf("Sending out VID advt for tree 1 on %s \n",interfaceNames[i]);
                //system("echo ADVT MSG SENT from tree 1 [looped bc root has come up]: >> MSTC.txt");
                payloadLen = build_VID_ADVT_PAYLOAD(payload, interfaceNames[i],treeNo);
                char eth[20];
                if (payloadLen) {
                    ctrlSend(interfaceNames[i], payload, payloadLen);
                    system("echo ADVT MSG SENT from tree 1 [looped bc root has come up]: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");
                    sprintf(eth, "echo %s >> MSTC.txt", interfaceNames[i]);
                    system(eth);
                }
                free(payload);
            }
        // Sending join msg for tree 2
        printf("Sending out join msg for tree 2 \n");
        system("echo Sending out join msg for tree 2 >> MSTC.txt");
        uint8_t *payload1 = NULL;
        int payloadLen1 = 0;
        payload1 = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
        payloadLen1 = build_JOIN_MSG_PAYLOAD(payload1);
        system("echo hit init join for 2nd tree >> MSTC.txt");

        if (payloadLen1) {
            int i = 0;
            char eth[20];
            for (; i < numberOfInterfaces; ++i) {
                ctrlSend(interfaceNames[i], payload1, payloadLen1);
                printf("Sending out join msg for tree 2 on interface %s\n",interfaceNames[i]);
                system("echo -n `date +\"JOIN MSG SENT AT [%H:%M:%S:%N] ON\"` >> MSTC.txt");
                sprintf(eth, "echo ' [%s]' >> MSTC.txt", interfaceNames[i]);
                system(eth);
            }
        }
        free(payload1);
    }
    else if (isSecondaryRoot) {
            struct vid_addr_tuple *new_node = (struct vid_addr_tuple *) calloc(1, sizeof(struct vid_addr_tuple));

            // Create VID structure for user-defined root VID
            strncpy(new_node->vid_addr, secondaryRootVID, strlen(secondaryRootVID));
            strcpy(new_node->eth_name,
                   "self");    // own interface, so mark it as self, will be helpful while tracking own VIDs.
            new_node->last_updated = -1;                // -1 here because root ID should not be removed.
            new_node->port_status = PVID_PORT;
            new_node->next = NULL;
            new_node->isNew = true;
            new_node->path_cost = PATH_COST;
            new_node->membership = 2;

            // Shashank : create another method like add_entry_LL_2 for adding to the secondary root VID linked list
            // Add into VID Table.
            add_entry_LL2(new_node);

            int i = 0;
            uint8_t *payload = NULL;
            uint8_t payloadLen;
            int treeNo = 2;
            //treeStable = true;
            for (; i < numberOfInterfaces; i++) {
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                // Shashank : When building the VID_ADVT payload send both the VID linkedlists
                payloadLen = build_VID_ADVT_PAYLOAD(payload, interfaceNames[i],treeNo);
                if (payloadLen) {
                    ctrlSend(interfaceNames[i], payload, payloadLen);
                    system("echo ADVT MSG SENT [looped bc root has come up]: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");
                    char eth[20];
                    sprintf(eth, "echo %s >> MSTC.txt", interfaceNames[i]);
                    system(eth);
                }
                free(payload);
            }
        // Sending join msg for tree 1
        uint8_t *payload1 = NULL;
        int payloadLen1 = 0;
        payload1 = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
        payloadLen1 = build_JOIN_MSG_PAYLOAD(payload1);
        system("echo hit init join for 1st tree >> MSTC.txt");

        if (payloadLen1) {
            int i = 0;
            for (; i < numberOfInterfaces; ++i) {
                ctrlSend(interfaceNames[i], payload1, payloadLen1);
                system("echo -n `date +\"JOIN MSG SENT AT [%H:%M:%S:%N] ON\"` >> MSTC.txt");
                char eth[20];
                sprintf(eth, "echo ' [%s]' >> MSTC.txt", interfaceNames[i]);
                system(eth);
            }
        }
        free(payload1);
    }
    else {
        //-----------------------initial join--------------------------
        uint8_t *payload = NULL;
        int payloadLen = 0;

        payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
        payloadLen = build_JOIN_MSG_PAYLOAD(payload);
        system("echo hit init join for both the trees >> MSTC.txt");

        if (payloadLen) {
            int i = 0;
            for (; i < numberOfInterfaces; ++i) {
                ctrlSend(interfaceNames[i], payload, payloadLen);
                system("echo -n `date +\"JOIN MSG SENT AT [%H:%M:%S:%N] ON\"` >> MSTC.txt");
                char eth[20];
                sprintf(eth, "echo ' [%s]' >> MSTC.txt", interfaceNames[i]);
                system(eth);
            }
        }
        free(payload);

        //-----------------------inital join--------------------------
    }
    //----------------------------------------------------------------------------


    while (true) {
        time(&time_advt_fin);
        if(treetoUse == 1) {
            if(!treeStable && !isPrimary_VID_Table_Empty()){
                system("echo 'Primary root is up\n'  >> MSTC.txt");
                treeStable = true;
            }
            if(treeStable){
                if (isPrimary_VID_Table_Empty()) {
                    //printf("Primary root failure, Switching to secondary root");
                    system("echo -n `date +\"PRIMARY ROOT IS DOWN, SWITCHING TO SECONDARY ROOT AT [%H:%M:%S:%6N]\"` >> MSTC.txt");
                    treetoUse = 2;
                }
            }
        }

        struct interface_tracker_t *currentInterface = interface_head;
        while (currentInterface != NULL) {
            if (!checkInterfaceIsActive(currentInterface->eth_name)) {
                /*just for recording purposes*/
                 if (currentInterface->isUP == true) {
                    system("echo -n `date +\"A LINK FAILURE WAS FOUND AT [%H:%M:%S:%6N] ON\"` >> linkFail.txt");
                    char ethFail[25];
                    sprintf(ethFail, "echo ' [%s]' >> linkFail.txt", currentInterface->eth_name);
                    system(ethFail);
                }
                /*end of recording*/

                currentInterface->isUP = false;
            } else {
                currentInterface->isUP = true;
            }

            if (currentInterface->isUP == false) {
                struct vid_addr_tuple *currentVID = getInstance_vid_tbl_LL();
                while (currentVID != NULL) {
                    if (strcmp(currentVID->eth_name, currentInterface->eth_name) == 0) {
                        currentVID->last_updated -= 100000;
                    }
                    currentVID = currentVID->next;
                }
                struct vid_addr_tuple *currentVID2 = getInstance_vid_tbl_LL2();
                while (currentVID2 != NULL) {
                    if (strcmp(currentVID2->eth_name, currentInterface->eth_name) == 0) {
                        currentVID2->last_updated -= 100000;
                    }
                    currentVID2 = currentVID2->next;
                }

            }
            currentInterface = currentInterface->next;
        }
        memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
        int numberOfDeletions = checkForFailuresPrimary(deletedVIDs);
        bool hasCPVIDDeletions = checkForFailuresPrimaryCPVID();

        if (numberOfDeletions > 0) {
            int treeNo = 1;
            uint8_t *payload = NULL;
            int payloadLen = 0;

            memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
            int numberOfInterfaces = getActiveInterfaces(interfaceNames);

            int i = 0;
            for (; i < numberOfInterfaces; i++) {
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                payloadLen = build_VID_CHANGE_PAYLOAD(payload, interfaceNames[i], deletedVIDs, numberOfDeletions,treeNo);
                if (payloadLen) {
                    ctrlSend(interfaceNames[i], payload, payloadLen);
                    system("echo VID CHANGE MSG SENT [bc VID failure]: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");
                }
                free(payload);
            }

            // Also check CPVID Table.
            i = 0;
            for (; i < numberOfDeletions; i++) {
                delete_entry_cpvid_LL(deletedVIDs[i]); //NS has exceeded expiry time and have not received hello
            }
            struct vid_addr_tuple *c1 = getInstance_vid_tbl_LL();
            if (c1 != NULL) {
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                print_entries_LL();
                payloadLen = build_VID_ADVT_PAYLOAD(payload, c1->eth_name, treeNo);
                // NS announcing my parent that I have a PVID from him
                if (payloadLen) {
                    ctrlSend(c1->eth_name, payload, payloadLen);

                    system("echo ADVT MSG SENT [bc deletions]: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");
                    char eth[20];
                    sprintf(eth, "echo %s >> MSTC.txt", c1->eth_name);
                    system(eth);
                }
                free(payload);
            }

            print_entries_LL();                     // MAIN VID TABLE
            print_entries_bkp_LL();                 // BKP VID TABLE
            print_entries_cpvid_LL();               // CHILD PVID TABLE
            print_entries_lbcast_LL();              // LOCAL HOST PORTS
        }

        memset(deletedVIDs2, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
        int numberOfDeletions2 = checkForFailuresSecondary(deletedVIDs2);
        bool hasCPVIDDeletions2 = checkForFailuresSecondaryCPVID();
        if (numberOfDeletions2 > 0) {
            int treeNo = 2;
            uint8_t *payload = NULL;
            int payloadLen = 0;

            memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
            int numberOfInterfaces = getActiveInterfaces(interfaceNames);
            int i = 0;
            for (; i < numberOfInterfaces; i++) {
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                payloadLen = build_VID_CHANGE_PAYLOAD(payload, interfaceNames[i], deletedVIDs2, numberOfDeletions2,treeNo);
                if (payloadLen) {
                    ctrlSend(interfaceNames[i], payload, payloadLen);
                    system("echo VID CHANGE MSG SENT [bc VID failure]: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");
                }
                free(payload);
            }

            // Also check CPVID Table.
            i = 0;
            for (; i < numberOfDeletions2; i++) {
                delete_entry_cpvid_LL2(deletedVIDs2[i]); //NS has exceeded expiry time and have not received hello
            }
            struct vid_addr_tuple *c1 = getInstance_vid_tbl_LL2();
            if (c1 != NULL) {
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                print_entries_LL();
                payloadLen = build_VID_ADVT_PAYLOAD(payload, c1->eth_name, treeNo);
                // NS announcing my parent that I have a PVID from him
                if (payloadLen) {
                    ctrlSend(c1->eth_name, payload, payloadLen);

                    system("echo ADVT MSG SENT [bc deletions]: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");
                    char eth[20];
                    sprintf(eth, "echo %s >> MSTC.txt", c1->eth_name);
                    system(eth);
                }
                free(payload);
            }

            print_entries_LL2();                     // MAIN VID TABLE
            print_entries_bkp_LL2();                 // BKP VID TABLE
            print_entries_cpvid_LL2();               // CHILD PVID TABLE
            print_entries_lbcast_LL();              // LOCAL HOST PORTS
        }
        if ((double) (difftime(time_advt_fin, time_advt_beg) >= PERIODIC_HELLO_TIME)) {
            memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
            int numberOfInterfaces = getActiveInterfaces(interfaceNames);
            uint8_t *payload = NULL;
            int payloadLen = 0;
            int treeNo = 3;
            bool sendJoin = false;

            if (!isPrimary_VID_Table_Empty() && !isSecondary_VID_Table_Empty()) {
                treeNo = 0;
            } else {
                if (isPrimary_VID_Table_Empty()) {
                    sendJoin = true;
            } else {
                if (!isPrimaryRoot || (isPrimaryRoot && getInstance_cpvid_LL() != NULL)) {
                    treeNo = 1;
                    }
                }
                if (isSecondary_VID_Table_Empty()) {
                    sendJoin = true;

                } else {
                    if (!isSecondaryRoot || (isSecondaryRoot && getInstance_cpvid_LL2() != NULL)) {
                        treeNo = 2;
                    }
                }
            }
            if(treeNo != 3){
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                payloadLen = build_PERIODIC_MSG_PAYLOAD(payload,treeNo);
                if (payloadLen) {
                    int i = 0;
                    for (; i < numberOfInterfaces; ++i) {
                        ctrlSend(interfaceNames[i], payload, payloadLen);
                    }
                }
                free(payload);
            }
            if(sendJoin){
                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                payloadLen = build_JOIN_MSG_PAYLOAD(payload);
                system("echo hit empty join >> MSTC.txt");
                if (payloadLen) {
                    int i = 0;
                    for (; i < numberOfInterfaces; ++i) {
                        ctrlSend(interfaceNames[i], payload, payloadLen);
                        system("echo JOIN MSGGG SENT: >> MSTC.txt");
                        system("date +%H:%M:%S:%N >> MSTC.txt");
                        char eth[20];
                        sprintf(eth, "echo %s >> MSTC.txt", interfaceNames[i]);
                        system(eth);
                    }
                }
                free(payload);
            }
            time(&time_advt_beg);
            time(&time_advt_beg2);

        }

        //start the process of receiving MT_PDU's
        socklen_t addr_len = sizeof(src_addr);

        int i = 0;
        for (i = 0; i < MAX_BUFFER_SIZE; i++) {
            recvBuffer[i] = '\0';
        }

        //recvfrom - receive a message from a socket, recvBuffer is where the PDU is stored
        recv_len = recvfrom(sockCtrl, recvBuffer, MAX_BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *) &src_addr,
                            &addr_len);

        //char sizeOfBuffer[5];
        //sprintf(sizeOfBuffer, "echo %d >> MSTC.txt", recv_len);
        //system(sizeOfBuffer);

        if (recv_len > 0) {
            char recvOnEtherPort[5];

            // read ethernet header
            eheader = (struct ether_header *) recvBuffer;
            //printf("Source MAC: %s\n", ether_ntoa((struct ether_addr *) &eheader->ether_shost));
            //printf("Destination MAC: %s\n", ether_ntoa((struct ether_addr *)&eheader->ether_dhost));

            if_indextoname(src_addr.sll_ifindex, recvOnEtherPort);

            char ctrlInterface[] = "eth0";

            // ignore frames that are from the control interface.
            if ((strcmp(recvOnEtherPort, ctrlInterface)) == 0) {
                continue;
            } else {
                // This is a MTP frame so, incase this port is in Local host broadcast table remove it.
                delete_entry_lbcast_LL(recvOnEtherPort);
            }
            int treeNo;

            //switch statement for 14th index of recvBuffer (frame),
            switch (recvBuffer[14]) {
                //printf("\n MTP received VALID ctrl message");
                /*
                 *MT_JOIN - this message is sent by a switch that hears  MT_ADVT messages on its ports and desires to join on one of the tree branches the *advertised in the MT_VIDs.
                */
                case MTP_TYPE_JOIN_MSG: {

                        system("echo JOIN MSG RECIEVED : >> MSTC.txt");
                        system("date +%H:%M:%S:%N >> MSTC.txt");

                        char eth[20];
                        sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                        system(eth);

                        //if the Primary VID table is empty, we can't be sending ADVT's out, we don't have anything to ADVT!
                        if (!isPrimary_VID_Table_Empty()) {
                            int treeNo = 1;
                            uint8_t *payload = NULL;
                            int payloadLen = 0;
                            payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                            // recvOnEtherPort - Payload destination will same from where Join message has orginated.
                            payloadLen = build_VID_ADVT_PAYLOAD(payload, recvOnEtherPort,treeNo);
                            if (payloadLen) {
                                ctrlSend(recvOnEtherPort, payload, payloadLen);
                                system("echo -n ADVT MSG SENT [bc JOIN recieved]: >> MSTC.txt");
                                system("date +%H:%M:%S:%N >> MSTC.txt");
                                char eth[20];
                                sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                                system(eth);
                            }

                            free(payload);
                            // Send VID Advt
                        }
                    if (!isSecondary_VID_Table_Empty()) {
                        uint8_t *payload = NULL;
                        int payloadLen = 0;
                        int treeNo = 2;
                        payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                        // recvOnEtherPort - Payload destination will same from where Join message has orginated.
                        payloadLen = build_VID_ADVT_PAYLOAD(payload, recvOnEtherPort,treeNo);
                        if (payloadLen) {
                            ctrlSend(recvOnEtherPort, payload, payloadLen);
                            system("echo ADVT MSG SENT [bc JOIN recieved]: >> MSTC.txt");
                            system("date +%H:%M:%S:%N >> MSTC.txt");
                            char eth[20];
                            sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                            system(eth);
                        }

                        free(payload);
                        // Send VID Advt
                    }


                }

                    break;

                    /*
                     *MT_HELLO – this message is a keep-alive indicator and issued periodically on all MTP_ports by a switch running the MTP. A root switch that sends *this message will carry the MT_VID of the root switch. A non-root switch that sends this message will carry all of its MT_VIDs.
                    */
                case MTP_TYPE_PERODIC_MSG: {

                    treeNo = (int) recvBuffer[15];

                    if (treeNo == 0 || treeNo == 1) {
                        struct ether_addr src_mac;
                        bool retMainVID, retCPVID;
                        memcpy(&src_mac, (struct ether_addr *) &eheader->ether_shost, sizeof(struct ether_addr));
                        retMainVID = update_hello_time_LL(&src_mac);
                        retCPVID = update_hello_time_cpvid_LL(&src_mac);

                        if (retMainVID || retCPVID) {
                            //Hello Keep-alive recieved, empty conditional is inefficent...need to fix this at some point
                        }

                            //10/18/17 - delay in convergence occuring because this is the first occurance of a join happening [fixed]
                        else {
                            if (!isPrimaryRoot) {
                                uint8_t *payload = NULL;
                                int payloadLen = 0;
                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                                payloadLen = build_JOIN_MSG_PAYLOAD(payload);
                                if (payloadLen) {
                                    ctrlSend(recvOnEtherPort, payload, payloadLen);
                                    system("echo JOIN MSG SENT [bc hello recieved]: >> MSTC.txt");
                                    system("date +%H:%M:%S:%N >> MSTC.txt");

                                    char eth[20];
                                    sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                                    system(eth);
                                }
                                free(payload);
                            }
                        }
                    }
                    if(treeNo == 0 || treeNo == 2) {
                        struct ether_addr src_mac2;
                        bool retMainVID2, retCPVID2;
                        memcpy(&src_mac2, (struct ether_addr *) &eheader->ether_shost, sizeof(struct ether_addr));
                        retMainVID2 = update_hello_time_LL2(&src_mac2);
                        retCPVID2 = update_hello_time_cpvid_LL2(&src_mac2);

                        if (retMainVID2 || retCPVID2) {
                            //Hello Keep-alive recieved, empty conditional is inefficent...need to fix this at some point
                        }

                            //10/18/17 - delay in convergence occuring because this is the first occurance of a join happening [fixed]
                        else {
                            if (!isSecondaryRoot) {
                                uint8_t *payload = NULL;
                                int payloadLen = 0;
                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                                payloadLen = build_JOIN_MSG_PAYLOAD(payload);
                                if (payloadLen) {
                                    ctrlSend(recvOnEtherPort, payload, payloadLen);
                                    system("echo JOIN MSG SENT [bc hello recieved]: >> MSTC.txt");
                                    system("date +%H:%M:%S:%N >> MSTC.txt");

                                    char eth[20];
                                    sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                                    system(eth);
                                }
                                free(payload);
                            }
                        }
                    }
                }
                    break;

                    /*
                     *MT_ADVT – A switch which already has an MT_VID, when it receives an MT_NULL message on a port, will move that port to be an MT_port. It will *then send an MT_ADVT message that contains a unique MT_VID that the new switch can use. This MT_VID will be one of its The MT_ADVT message can *also be sent on receiving an MT_JOIN message defined next.
                    */
                case MTP_TYPE_VID_ADVT: {
                    treeNo = (int) recvBuffer[17];
                    //system("echo Recieved tree no is >> MSTC.txt");
                    //char treeprint[30];
                    //sprintf(treeprint, "echo %d >> MSTC.txt", treeNo);
                    //system(treeprint);
                    //printf("Advt msg recieved from %d \n",treeNo);
                    //system("echo -n `date +\"\n\nADVT MSG RECIEVED AT [%H:%M:%S:%N] ON\"` >> MSTC.txt");
                    if (treeNo == 1) {
                        char *mainVIDs[3] = {NULL, NULL, NULL};
                        system("echo -n `date +\"\n\nADVT MSG RECIEVED FROM TREE 1 AT [%H:%M:%S:%N] ON\"` >> MSTC.txt");
                        char eth[30];
                        sprintf(eth, "echo -n ' [%s]' >> MSTC.txt", recvOnEtherPort);
                        system(eth);

                        if (sizeOfPrimaryVIDTable() >= 3) {
                            struct vid_addr_tuple *currentTable = getInstance_vid_tbl_LL();
                            int i = 0;
                            for (i = 0; i < 3; i++) {
                                mainVIDs[i] = currentTable->vid_addr;
                                currentTable = currentTable->next;
                            }
                        }

                        //second byte of the MTP payload is the Operation field, adding a VID (VID_ADD) = 1, removing a VID = 2
                        uint8_t operation = (uint8_t) recvBuffer[15];
                        printf("Operation is %u",operation);

                        if (operation == VID_ADD) {
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16];
                            int tracker = 18;
                            bool hasAdditions = false;
                            bool mainVIDTableChange = false;

                            system("echo -n ' [ADD] ' >> MSTC.txt");


                            //running through all of the VID's recieved in the ADVT
                            while (numberVIDS != 0) {
                                // <VID_PATH_COST>
                                uint8_t path_cost = (uint8_t) recvBuffer[tracker];
                                // next byte
                                tracker = tracker + 1;

                                // <VID_ADDR_LEN>
                                uint8_t vid_len = recvBuffer[tracker];
                                // next byte
                                tracker = tracker + 1;

                                // <VID_ADDR>
                                char vid_addr[vid_len];
                                memset(vid_addr, '\0', vid_len);
                                strncpy(vid_addr, &recvBuffer[tracker], vid_len);
                                vid_addr[vid_len] = '\0';

                                //stack smash
                                char checkVID[100];
                                sprintf(checkVID, "echo -n ' %s' >> MSTC.txt", vid_addr);
                                system(checkVID);

                                // next byte after length of VID
                                tracker += vid_len;

                                //returns an int to tell protocol if VID is a child of a current main or backup table VID
                                int ret = isPrimaryChild(vid_addr);

                                // if VID child check, incase part of PVID add to Child PVID table.
                                if (ret == 1) {
                                    // if this is the first VID in the table and is a child, we have to add into child PVID Table
                                    if (numberVIDS == (uint8_t) recvBuffer[16]) {
                                        // if same first ID
                                        struct child_pvid_tuple *new_cpvid = (struct child_pvid_tuple *) calloc(1,
                                                                                                                sizeof(struct child_pvid_tuple));

                                        // Fill data.
                                        strncpy(new_cpvid->vid_addr, vid_addr, strlen(vid_addr));
                                        strncpy(new_cpvid->child_port, recvOnEtherPort, strlen(recvOnEtherPort));
                                        memcpy(&new_cpvid->mac, (struct ether_addr *) &eheader->ether_shost,
                                               sizeof(struct ether_addr));
                                        new_cpvid->next = NULL;
                                        new_cpvid->last_updated = time(0); // last updated time

                                        //Add into child PVID table, if already there update it if any changes.
                                        if (add_entry_cpvid_LL(new_cpvid)) {
                                            system("echo [Added to CPVID table] >> MSTC.txt");
                                        } else {
                                            //if already there deallocate node memory
                                            free(new_cpvid);
                                            //update CPVID time here
                                        }
                                    }
                                }

                                    // Add to Main VID Table, if not a child, make it PVID if there is no better path already in the table.
                                else if (ret == -1) {
                                    // Allocate memory and intialize(calloc).
                                    struct vid_addr_tuple *new_node = (struct vid_addr_tuple *) calloc(1,
                                                                                                       sizeof(struct vid_addr_tuple));

                                    // Fill in data for new VID
                                    strncpy(new_node->vid_addr, vid_addr, strlen(vid_addr)); //VID Address
                                    strncpy(new_node->eth_name, recvOnEtherPort,
                                            strlen(recvOnEtherPort)); //Incoming Ethernet Port Name
                                    new_node->last_updated = time(0); // current timestamp
                                    new_node->port_status = PVID_PORT; //Port Type
                                    new_node->next = NULL; //Linked List Hook
                                    new_node->isNew = true; //Is New
                                    new_node->membership = 0;     // Intialize with '0', will find outpreference based on cost during add method.
                                    new_node->path_cost = (uint8_t) path_cost; //Path Cost
                                    memcpy(&new_node->mac, (struct ether_addr *) &eheader->ether_shost,
                                           sizeof(struct ether_addr)); //Source MAC address of VID ADVT

                                    // Add into VID Table, if addition success, update all other connected peers about the change.
                                    int mainVIDTracker = add_entry_LL(new_node);

                                    if (mainVIDTracker > 0) {

                                        if (mainVIDTracker == 1) {
                                            system("echo [added to Main VID Table] >> MSTC.txt");
                                            hasAdditions = true;
                                        }

                                        // If peer has VID derived from me earlier and has a change now.
                                        if (numberVIDS == (uint8_t) recvBuffer[16]) {
                                            // Check PVID used by peer is a derived PVID from me.
                                            delete_MACentry_cpvid_LL(&new_node->mac);
                                        }
                                    }
                                }

                                numberVIDS--;
                            }

                            if (sizeOfPrimaryVIDTable() > 3 && hasAdditions && mainVIDs[2] != NULL) {
                                memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
                                int numberToDelete = checkForPrimaryVIDTableChanges(mainVIDs, deletedVIDs);

                                if (numberToDelete > 0) {
                                    uint8_t *payload = NULL;
                                    int payloadLen = 0;

                                    memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
                                    int numberOfInterfaces = getActiveInterfaces(interfaceNames);

                                    int i = 0;
                                    for (; i < numberOfInterfaces; i++) {
                                        payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                                        payloadLen = build_VID_CHANGE_PAYLOAD(payload, interfaceNames[i], deletedVIDs,
                                                                              numberToDelete, treeNo);

                                        if (payloadLen) {
                                            ctrlSend(interfaceNames[i], payload, payloadLen);
                                            system("echo ADVT CHANGE MSG SENT [bc MAIN TABLE CHANGE]: >> MSTC.txt");
                                            system("date +%H:%M:%S:%N >> MSTC.txt");
                                        }
                                        free(payload);
                                    }
                                }
                            }

                            if (hasAdditions) {
                                uint8_t *payload = NULL;
                                int payloadLen = 0;
                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                                // recvOnEtherPort - Payload destination will same from where ADVT message has orginated.
                                payloadLen = build_VID_ADVT_PAYLOAD(payload, recvOnEtherPort,treeNo);

                                if (payloadLen) {
                                    ctrlSend(recvOnEtherPort, payload, payloadLen);

                                    system("echo ADVT MSG SENT back [bc ADVT recieved and added to main VID table]: >> MSTC.txt");
                                    system("date +%H:%M:%S:%N >> MSTC.txt");
                                    char eth[20];
                                    sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                                    system(eth);
                                }
                                free(payload);

                                // ----------------------------------REST OF INTERFACES START------------------------------
                                memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
                                int numberOfInterfaces = getActiveInterfaces(interfaceNames);
                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);

                                int i = 0;
                                for (; i < numberOfInterfaces; ++i) {

                                    if ((strcmp(recvOnEtherPort, interfaceNames[i])) == 0) {
                                        continue;
                                    }

                                    payloadLen = build_VID_ADVT_PAYLOAD(payload, interfaceNames[i],treeNo);

                                    if (payloadLen) {
                                        ctrlSend(interfaceNames[i], payload, payloadLen);

                                        system("echo ADVT MSG SENT [looped bc ADVT recieved]: >> MSTC.txt");
                                        system("date +%H:%M:%S:%N >> MSTC.txt");
                                        char eth[20];
                                        sprintf(eth, "echo %s >> MSTC.txt", interfaceNames[i]);
                                        system(eth);
                                    }
                                }
                                free(payload);
                                // ----------------------------------REST OF INTERFACES END------------------------------
                            }
                        } else if (operation == VID_DEL) {
                            // Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16];

                            // delete all local entries, get a list and send to others who derive from this VID.
                            memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);

                            //3-22
                            uint8_t numberOfDeletions = numberVIDS;
                            bool hasDeletions = false;
                            bool needToCommDeletions = false;

                            system("echo -n ' [DEL] ' >> MSTC.txt");

                            int i = 0;
                            int tracker = 18;
                            while (i < numberOfDeletions) {
                                //<VID_ADDR_LEN>
                                uint8_t vid_len = recvBuffer[tracker];

                                // next byte, make tracker point to VID_ADDR
                                tracker = tracker + 1;

                                deletedVIDs[i] = (char *) calloc(vid_len, sizeof(char));
                                strncpy(deletedVIDs[i], &recvBuffer[tracker], vid_len);
                                recvBuffer[vid_len] = '\0';

                                char checkVID[100];
                                sprintf(checkVID, "echo -n ' , %s' >> MSTC.txt", deletedVIDs[i]);
                                system(checkVID);

                                hasDeletions = delete_entry_LL(deletedVIDs[i]);
                                delete_entry_cpvid_LL(deletedVIDs[i]);

                                if (hasDeletions) {
                                    needToCommDeletions = true;
                                }

                                char checkResult[10];
                                sprintf(checkResult, "echo -n ' Status : %s' >> MSTC.txt", hasDeletions ? "true" : "false");
                                system(checkResult);

                                tracker += vid_len;
                                i++;
                            }

                            uint8_t *payload;
                            int payloadLen;

                            // Only if we have deletions we will be advertising it to our connected peers.
                            if (needToCommDeletions) {
                                memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
                                int numberOfInterfaces = getActiveInterfaces(interfaceNames);

                                i = 0;
                                for (; i < numberOfInterfaces; i++) {
                                    payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);

                                    payloadLen = build_VID_CHANGE_PAYLOAD(payload, interfaceNames[i], deletedVIDs,
                                                                          numberOfDeletions, treeNo);
                                    if (payloadLen) {
                                        ctrlSend(interfaceNames[i], payload, payloadLen);
                                        system("echo ADVT CHANGE MSG SENT [bc DEL received]: >> MSTC.txt");
                                        system("date +%H:%M:%S:%N >> MSTC.txt");
                                    }
                                    free(payload);
                                }

                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);

                                struct vid_addr_tuple *c1 = getInstance_vid_tbl_LL();
                                if (c1 != NULL) {
                                    payloadLen = build_VID_ADVT_PAYLOAD(payload, c1->eth_name,treeNo);
                                    if (payloadLen) {
                                        ctrlSend(c1->eth_name, payload, payloadLen);
                                        system("echo ADVT MSG SENT: >> MSTC.txt");
                                        system("date +%H:%M:%S:%N >> MSTC.txt");
                                        //printf("Sending %s\n", c1->vid_addr);

                                        char eth[20];
                                        sprintf(eth, "echo %s >> MSTC.txt", c1->eth_name);
                                        system(eth);
                                    }
                                    free(payload);
                                }
                            }
                        } else {
                            printf("Unknown VID Advertisment\n");
                            printf("Operation is %u",operation);
                            system("echo UNKNOWN ADVT RECEIVED: >> MSTC.txt");
                        }

                        print_entries_LL();
                        print_entries_bkp_LL();
                        print_entries_cpvid_LL();
                        print_entries_lbcast_LL();
                        printf("----------------------------------------------------------\n");
                    }
                    else{

                        char *mainVIDs[3] = {NULL, NULL, NULL};

                        system("echo -n `date +\"\n\nADVT MSG RECIEVED FROM TREE 2 AT [%H:%M:%S:%N] ON\"` >> MSTC.txt");
                        char eth[30];
                        sprintf(eth, "echo -n ' [%s]' >> MSTC.txt", recvOnEtherPort);
                        system(eth);

                        if (sizeOfSecondaryVIDTable() >= 3) {
                            struct vid_addr_tuple *currentTable = getInstance_vid_tbl_LL2();
                            int i = 0;
                            for (i = 0; i < 3; i++) {
                                mainVIDs[i] = currentTable->vid_addr;
                                currentTable = currentTable->next;
                            }
                        }

                        //second byte of the MTP payload is the Operation field, adding a VID (VID_ADD) = 1, removing a VID = 2
                        uint8_t operation = (uint8_t) recvBuffer[15];
                        printf(" Operation is %u \n", operation);
                        if (operation == VID_ADD) {
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16];
                            int tracker = 18;
                            bool hasAdditions = false;
                            bool mainVIDTableChange = false;

                            system("echo -n ' [ADD] ' >> MSTC.txt");

                            //running through all of the VID's recieved in the ADVT
                            while (numberVIDS != 0) {
                                // <VID_PATH_COST>
                                uint8_t path_cost = (uint8_t) recvBuffer[tracker];
                                // next byte
                                tracker = tracker + 1;

                                // <VID_ADDR_LEN>
                                uint8_t vid_len = recvBuffer[tracker];
                                // next byte
                                tracker = tracker + 1;

                                // <VID_ADDR>
                                char vid_addr[vid_len];
                                memset(vid_addr, '\0', vid_len);
                                strncpy(vid_addr, &recvBuffer[tracker], vid_len);
                                vid_addr[vid_len] = '\0';

                                //stack smash
                                char checkVID[100];
                                sprintf(checkVID, "echo -n ' %s' >> MSTC.txt", vid_addr);
                                system(checkVID);

                                // next byte after length of VID
                                tracker += vid_len;

                                //returns an int to tell protocol if VID is a child of a current main or backup table VID
                                int ret = isSecondaryChild(vid_addr);

                                // if VID child check, incase part of PVID add to Child PVID table.
                                if (ret == 1) {
                                    // if this is the first VID in the table and is a child, we have to add into child PVID Table
                                    if (numberVIDS == (uint8_t) recvBuffer[16]) {
                                        // if same first ID
                                        struct child_pvid_tuple *new_cpvid = (struct child_pvid_tuple *) calloc(1,
                                                                                                                sizeof(struct child_pvid_tuple));

                                        // Fill data.
                                        strncpy(new_cpvid->vid_addr, vid_addr, strlen(vid_addr));
                                        strncpy(new_cpvid->child_port, recvOnEtherPort, strlen(recvOnEtherPort));
                                        memcpy(&new_cpvid->mac, (struct ether_addr *) &eheader->ether_shost,
                                               sizeof(struct ether_addr));
                                        new_cpvid->next = NULL;
                                        new_cpvid->last_updated = time(0); // last updated time

                                        //Add into child PVID table, if already there update it if any changes.
                                        if (add_entry_cpvid_LL2(new_cpvid)) {
                                            system("echo [Added to CPVID table] >> MSTC.txt");
                                        } else {
                                            //if already there deallocate node memory
                                            free(new_cpvid);
                                            //update CPVID time here
                                        }
                                    }
                                }

                                    // Add to Main VID Table, if not a child, make it PVID if there is no better path already in the table.
                                else if (ret == -1) {
                                    // Allocate memory and intialize(calloc).
                                    struct vid_addr_tuple *new_node = (struct vid_addr_tuple *) calloc(1,
                                                                                                       sizeof(struct vid_addr_tuple));

                                    // Fill in data for new VID
                                    strncpy(new_node->vid_addr, vid_addr, strlen(vid_addr)); //VID Address
                                    strncpy(new_node->eth_name, recvOnEtherPort,
                                            strlen(recvOnEtherPort)); //Incoming Ethernet Port Name
                                    new_node->last_updated = time(0); // current timestamp
                                    new_node->port_status = PVID_PORT; //Port Type
                                    new_node->next = NULL; //Linked List Hook
                                    new_node->isNew = true; //Is New
                                    new_node->membership = 0;     // Intialize with '0', will find outpreference based on cost during add method.
                                    new_node->path_cost = (uint8_t) path_cost; //Path Cost
                                    memcpy(&new_node->mac, (struct ether_addr *) &eheader->ether_shost,
                                           sizeof(struct ether_addr)); //Source MAC address of VID ADVT

                                    // Add into VID Table, if addition success, update all other connected peers about the change.
                                    int mainVIDTracker = add_entry_LL2(new_node);

                                    if (mainVIDTracker > 0) {

                                        if (mainVIDTracker == 1) {
                                            system("echo [added to Main VID Table] >> MSTC.txt");
                                            hasAdditions = true;
                                        }

                                        // If peer has VID derived from me earlier and has a change now.
                                        if (numberVIDS == (uint8_t) recvBuffer[16]) {
                                            // Check PVID used by peer is a derived PVID from me.
                                            delete_MACentry_cpvid_LL2(&new_node->mac);
                                        }
                                    }
                                }

                                numberVIDS--;
                            }

                            if (sizeOfSecondaryVIDTable() > 3 && hasAdditions && mainVIDs[2] != NULL) {
                                memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
                                int numberToDelete = checkForSecondaryVIDTableChanges(mainVIDs, deletedVIDs);

                                if (numberToDelete > 0) {
                                    uint8_t *payload = NULL;
                                    int payloadLen = 0;

                                    memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
                                    int numberOfInterfaces = getActiveInterfaces(interfaceNames);

                                    int i = 0;
                                    for (; i < numberOfInterfaces; i++) {
                                        payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                                        payloadLen = build_VID_CHANGE_PAYLOAD(payload, interfaceNames[i], deletedVIDs,
                                                                              numberToDelete, treeNo);

                                        if (payloadLen) {
                                            ctrlSend(interfaceNames[i], payload, payloadLen);
                                            system("echo ADVT CHANGE MSG SENT [bc MAIN TABLE CHANGE]: >> MSTC.txt");
                                            system("date +%H:%M:%S:%N >> MSTC.txt");
                                        }
                                        free(payload);
                                    }
                                }
                            }

                            if (hasAdditions) {
                                uint8_t *payload = NULL;
                                int payloadLen = 0;
                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);
                                // recvOnEtherPort - Payload destination will same from where ADVT message has orginated.
                                payloadLen = build_VID_ADVT_PAYLOAD(payload, recvOnEtherPort, treeNo);

                                if (payloadLen) {
                                    ctrlSend(recvOnEtherPort, payload, payloadLen);

                                    system("echo ADVT MSG SENT back [bc ADVT recieved and added to main VID table]: >> MSTC.txt");
                                    system("date +%H:%M:%S:%N >> MSTC.txt");
                                    char eth[20];
                                    sprintf(eth, "echo %s >> MSTC.txt", recvOnEtherPort);
                                    system(eth);
                                }
                                free(payload);

                                // ----------------------------------REST OF INTERFACES START------------------------------
                                memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
                                int numberOfInterfaces = getActiveInterfaces(interfaceNames);
                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);

                                int i = 0;
                                for (; i < numberOfInterfaces; ++i) {

                                    if ((strcmp(recvOnEtherPort, interfaceNames[i])) == 0) {
                                        continue;
                                    }

                                    payloadLen = build_VID_ADVT_PAYLOAD(payload, interfaceNames[i],treeNo);

                                    if (payloadLen) {
                                        ctrlSend(interfaceNames[i], payload, payloadLen);

                                        system("echo ADVT MSG SENT [looped bc ADVT recieved]: >> MSTC.txt");
                                        system("date +%H:%M:%S:%N >> MSTC.txt");
                                        char eth[20];
                                        sprintf(eth, "echo %s >> MSTC.txt", interfaceNames[i]);
                                        system(eth);
                                    }
                                }
                                free(payload);
                                // ----------------------------------REST OF INTERFACES END------------------------------
                            }
                        } else if (operation == VID_DEL) {
                            // Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16];

                            // delete all local entries, get a list and send to others who derive from this VID.
                            memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);

                            //3-22
                            uint8_t numberOfDeletions = numberVIDS;
                            bool hasDeletions = false;
                            bool needToCommDeletions = false;

                            system("echo -n ' [DEL] ' >> MSTC.txt");

                            int i = 0;
                            int tracker = 18;
                            while (i < numberOfDeletions) {
                                //<VID_ADDR_LEN>
                                uint8_t vid_len = recvBuffer[tracker];

                                // next byte, make tracker point to VID_ADDR
                                tracker = tracker + 1;

                                deletedVIDs[i] = (char *) calloc(vid_len, sizeof(char));
                                strncpy(deletedVIDs[i], &recvBuffer[tracker], vid_len);
                                recvBuffer[vid_len] = '\0';

                                char checkVID[100];
                                sprintf(checkVID, "echo -n ' %s' >> MSTC.txt", deletedVIDs[i]);
                                system(checkVID);

                                hasDeletions = delete_entry_LL2(deletedVIDs[i]);
                                delete_entry_cpvid_LL2(deletedVIDs[i]);

                                if (hasDeletions) {
                                    needToCommDeletions = true;
                                }

                                char checkResult[10];
                                sprintf(checkResult, "echo -n ' %s' >> MSTC.txt", hasDeletions ? "true" : "false");
                                system(checkResult);

                                tracker += vid_len;
                                i++;
                            }

                            uint8_t *payload;
                            int payloadLen;

                            // Only if we have deletions we will be advertising it to our connected peers.
                            if (needToCommDeletions) {
                                memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
                                int numberOfInterfaces = getActiveInterfaces(interfaceNames);

                                i = 0;
                                for (; i < numberOfInterfaces; i++) {
                                    payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);

                                    payloadLen = build_VID_CHANGE_PAYLOAD(payload, interfaceNames[i], deletedVIDs,
                                                                          numberOfDeletions,treeNo);
                                    if (payloadLen) {
                                        ctrlSend(interfaceNames[i], payload, payloadLen);
                                        system("echo ADVT CHANGE MSG SENT [bc DEL received]: >> MSTC.txt");
                                        system("date +%H:%M:%S:%N >> MSTC.txt");
                                    }
                                    free(payload);
                                }

                                payload = (uint8_t *) calloc(1, MAX_BUFFER_SIZE);

                                struct vid_addr_tuple *c1 = getInstance_vid_tbl_LL2();
                                if (c1 != NULL) {
                                    payloadLen = build_VID_ADVT_PAYLOAD(payload, c1->eth_name, treeNo);
                                    if (payloadLen) {
                                        ctrlSend(c1->eth_name, payload, payloadLen);
                                        system("echo ADVT MSG SENT: >> MSTC.txt");
                                        system("date +%H:%M:%S:%N >> MSTC.txt");
                                        //printf("Sending %s\n", c1->vid_addr);

                                        char eth[20];
                                        sprintf(eth, "echo %s >> MSTC.txt", c1->eth_name);
                                        system(eth);
                                    }
                                    free(payload);
                                }
                            }
                        } else {

                            printf("Unknown VID Advertisment\n");
                            system("echo UNKNOWN ADVT RECEIVED from tree 2 %u: >> MSTC.txt");
                        }

                        print_entries_LL2();
                        print_entries_bkp_LL2();
                        print_entries_cpvid_LL2();
                        print_entries_lbcast_LL();
                        printf("----------------------------------------------------------\n");
                    }
                }
                    break;

                default:
                    system("echo UNKNOWN MSG RECIEVED: >> MSTC.txt");
                    system("date +%H:%M:%S:%N >> MSTC.txt");

                    printf("NON-MTP PDU RECIEVED: ");

                    printf("Unknown Packet\n");
                    break;
            }
        }
        //else printf("\n No CONTROL Messages");

        /* Receive data traffic */
        recv_len = recvfrom(sockData, recvBuffer, MAX_BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *) &src_addr,
                            &addr_len);
        if (recv_len > 0) {
            //printf("received data traffic\n");

            char recvOnEtherPort[5];

            if_indextoname(src_addr.sll_ifindex, recvOnEtherPort);
            char ctrlInterface[] = "eth0";

            // ignore frames that are from the control interface.
            if ((strcmp(recvOnEtherPort, ctrlInterface)) == 0) {
                continue;
            }

            // read ethernet header
            eheader = (struct ether_header *) recvBuffer;

            // read ethernet header
            /*printf("Source MAC: %s\n", ether_ntoa((struct ether_addr *) &eheader->ether_shost));
              printf("Destination MAC: %s\n", ether_ntoa((struct ether_addr *)&eheader->ether_dhost));
              printf("Message Type: %x\n", ntohs(eheader->ether_type));*/

            // Check if the data frame is a broadcast.
            if (strncmp(ether_ntoa((struct ether_addr *) &eheader->ether_dhost), "ff:ff:ff:ff:ff:ff", 17) == 0) {
                printf("Received broadcast frame\n");

                //SEND TO HOST PORTS
                struct local_bcast_tuple *current = getInstance_lbcast_LL();
                for (; current != NULL; current = current->next) {
                    // port should not be the same from where it received frame.
                    if (strcmp(current->eth_name, recvOnEtherPort) != 0) {
                        dataSend(current->eth_name, recvBuffer, recv_len);
                        printf("Sent to host %s\n", current->eth_name);
                    }
                }


                //SEND TO CHILD MTS'
                struct child_pvid_tuple *cpt;
                if(treetoUse ==1) {
                    cpt = getInstance_cpvid_LL();
                }
                else{
                    cpt = getInstance_cpvid_LL2();
                }
                for (; cpt != NULL; cpt = cpt->next) {
                    // port should not be the same from where it received frame.
                    if (strcmp(cpt->child_port, recvOnEtherPort) != 0) {
                        dataSend(cpt->child_port, recvBuffer, recv_len);
                        printf("Sent to child %s\n", cpt->child_port);
                    }
                }

                //SEND TO PARENT MTS ON PVID PORT (if it didn't originate from that location)
                if(treetoUse ==1) {
                    if (!isPrimaryRoot) {
                        struct vid_addr_tuple *vid_t = getInstance_vid_tbl_LL();
                        // port should not be the same from where it received frame.
                        if (strcmp(vid_t->eth_name, recvOnEtherPort) != 0) {
                            dataSend(vid_t->eth_name, recvBuffer, recv_len);
                            printf("Sent to PVID %s\n", vid_t->eth_name);
                        }
                    }
                }
                else{
                    if (!isSecondaryRoot) {
                        struct vid_addr_tuple *vid_t = getInstance_vid_tbl_LL2();
                        // port should not be the same from where it received frame.
                        if (strcmp(vid_t->eth_name, recvOnEtherPort) != 0) {
                            dataSend(vid_t->eth_name, recvBuffer, recv_len);
                            printf("Sent to PVID %s\n", vid_t->eth_name);
                        }
                    }
                }
            }
        }
        // check if there are any pending VID Adverts - this is interesting, what was written here at one point
    } //End of While loop
} //End of mtp_start() function

// get active interfaces on the node.
int getActiveInterfaces(char **ptr) {
    // find all interfaces on the node.
    int indexLen = 0;

    //structures that describe the network interfaces of the local system
    struct ifaddrs *ifaddr, *ifa;

    /*The getifaddrs() function creates a linked list of structures describing the network interfaces of the local system,
     *and stores the address of the first item of the list in *ifa.  The list consists of ifaddrs structures
     */
    if (getifaddrs(&ifaddr)) {
        perror("Error: getifaddrs Failed\n");
        exit(0);
    }

    // loop through the list, the last part moves the list to the next struct (interface) as long as there still is one (!NULL)
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        //if the interfaces has no address, continue
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        //family reaches into ifa_addr, which is a pointer to a struct sockaddr variable that describes the address family
        int family;
        family = ifa->ifa_addr->sa_family;

        // populate interface names, if interface is in the AF_INET family, UP, and if ethernet interface doesn't belong to control interface and Loopback interface.
        if (family == AF_INET && (strncmp(ifa->ifa_name, "lo", 2) != 0) && (ifa->ifa_flags & IFF_UP) != 0) {
            char networkIP[NI_MAXHOST];

            //IPv4 AF_INET sockets, grab the IP address from the general ifaddrs struct
            struct sockaddr_in *ipaddr = ((struct sockaddr_in *) ifa->ifa_addr);

            //Converts a numeric address into a text string suitable for presentation (family, source, destination, size)
            inet_ntop(AF_INET, &(ipaddr->sin_addr), networkIP, INET_ADDRSTRLEN);

            //manually checking the output of the binary-to-text output of inet_ntop
            //printf("IP address being checked for getActiveInt: %s\n", networkIP);

            //compares first three bytes to determine if its control interface from GENI (172.16.0.0/12)
            if (strncmp(networkIP, CTRL_IP, 3) == 0) {
                // skip, as it is control interface.
                //printf("we are skipping over the control addr %s\n", networkIP);
                continue;
            }

            //take the argument given to the function, pointer to memory the size of all the interfaces, give it as the dest arg in strncpy.
            ptr[indexLen] = (char *) calloc(strlen(ifa->ifa_name), sizeof(char));
            strncpy(ptr[indexLen], ifa->ifa_name, strlen(ifa->ifa_name));
            indexLen++;
        }
    }
    freeifaddrs(ifaddr);
    //printf("size of indexlen: %d\n", indexLen);
    return indexLen;
}


void learn_active_interfaces() {
    int numberOfInterfaces;
    char **iNames;
    struct interface_tracker_t *current = NULL;

    iNames = (char **) calloc(MAX_INTERFACES * MAX_INTERFACES, sizeof(char));
    memset(iNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);

    numberOfInterfaces = getActiveInterfaces(iNames);

    int i = 0;
    for (; i < numberOfInterfaces; i++) {
        struct interface_tracker_t *temp = (struct interface_tracker_t *) calloc(1, sizeof(struct interface_tracker_t));
        strncpy(temp->eth_name, iNames[i], strlen(iNames[i]));
        temp->isUP = true;
        temp->next = current;
        current = temp;
    }

    interface_head = current; //the key to all of this, remember this for linked lists!

    //printf("testing testing 123\n");
}


bool checkInterfaceIsActive(char *str) {
    // find all interfaces on the node.
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr)) {
        perror("Error: getifaddrs Failed\n");
        exit(0);
    }

    // loop through the list
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        int family;
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET && (strncmp(ifa->ifa_name, str, strlen(str)) == 0) && (ifa->ifa_flags & IFF_UP) != 0) {
            freeifaddrs(ifaddr);
            return true;
        }
    }
    freeifaddrs(ifaddr);

    return false;
}
