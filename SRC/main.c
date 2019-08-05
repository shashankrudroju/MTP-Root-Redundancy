/*
 *	Main.c
 *
 *  Created on: Sep 21, 2015, updated 2017-present
 *  Authors: Pranav Sai(pk6420@rit.edu), Peter Willis (pjw7904@rit.edu), Shashank Rudroju(sr1632@rit.edu)
 */

/* INCLUDED LIBRARIES */
#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
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
#include <mcheck.h>           // Adding this to check for memory issues/to catch and gracefully end on Ctrl C
#include "feature_payload.h"  // Adding custom functions from Feature Payload files
#include "mtp_send.h"         // Adding custom functions from MTP Send files

/* CUSTOM DEFINTIONS */
#define ETH_MTP_CTRL  0x8850  // MTP 802.3 Ethertype
#define ETH_UNI_DATA  0xff00  // MTP Unicast Traffic 802.3 Ethertype
#define MAX_VID_LIST  20      // Previously defined max number of VIDs, in use for now for memory allocation
#define CTRL_IP		    "172"   // First octet of IPv4 address given to GENI control interfaces (eth0)

/* GLOBAL VARIABLES */
bool isPrimaryRoot = false; // Flag to determine if a node is the secondary root of a meshed tree
bool isSecondaryRoot = false;	// Flag to determine if a node is the secondary root of a meshed tree
char* primaryRootVID = '\0';	// The VID on the root node that gives the
char* secondaryRootVID = '\0';
bool treeStable = false;
int  treetoUse = 1;

/* FUNCTION PROTOTYPES */
int getActiveInterfaces(char **);								// Function used to determine active interfaces on the node
void runMTP();																	// Function used to implement the Meshed Tree Protocol finite State machine
void floodFrame(uint8_t *, int, char *, int);			  // Function used to flood broadcast frames via the Primary VID Tree
void switchUnicastFrame(uint8_t*, int, char *, int); // Function used to switch unicast frames via the Host Address Table
void markMulticastFrame(char *);							  // Function used to identify and drop multicast frames
void sig_handler(int);                          // Function used to gracefully close the software (not used?)


/*
 * Function: main
 * ----------------------------
 *   Starts the process of getting the Meshed Tree Protocol operational on the Meshed Tree Switch (MTS)
 *
 *   argc: The number of arguments against main.c
 *   argv: Character array with arugments against main.c
 *
 *   returns: 0 (zero), ending the protocol implementation
 */
int main (int argc, char** argv)
{
    char **interfaceNames;

    //Shashank : The arguments will change..based on whether its a Primary root or a secondary root
    //Shashank : If its a primary root, the arguments will be ./main 1 <VID> else if its secondary ./main 2 <VID>, if its a non root then ./main 0
    if(argc < 2) // At least two arguments are necessary for MTP operation
    {
        printf("Error: Node spec or ROOT MTS ID missing. Format ./mtpd <non MTS/root MTS> <ROOT MTS ID>\n");
        printf("Error: 0 for non MTS, 1 for root MTS\n");
        exit(1);
    }

    /*
       * Check if Node is the Root MTS (arg of 1) or a non-MTS (arg of 0), atoi converts a string argument into an integer.
     * Example: ./mtpd 1 1 = I am the root switch, and my VID (the root VID) is 1
     */
    // Shashank : Add extra condition and check if its primary of secondary and update the appropriate isPrimaryRoot flag to true
    // Shashank : Also need to set the priority of primary root to 1
    if(atoi(argv[1]) == 1) //make sure this didn't change in the linux machine crash
    {
        isPrimaryRoot = true;
        primaryRootVID = argv[2]; // another argument has to be added that tells the program what the root VID is
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

    interfaceNames = (char**) calloc (MAX_INTERFACES * MAX_INTERFACES, sizeof(char)); // Allocates memory for interface names (ex: eth1)
    memset(interfaceNames, '\0', sizeof(char) * MAX_INTERFACES * MAX_INTERFACES);
    int numberOfInterfaces = getActiveInterfaces(interfaceNames); // Returns the number of interfaces on the MTS while populating interfaceNames

    /* All ints start as host interfaces, if we get a MTP CTRL frame from any interface later we remove it. */
    int i = 0;
    for (; i < numberOfInterfaces; i++)
    {
        /* Local Host Broadcast Table (Table 1/8) - Contains non-MTP intefaces via their interface name (ex: eth1) */
        struct local_bcast_tuple *new_node = (struct local_bcast_tuple*) calloc(1, sizeof(struct local_bcast_tuple));
        strncpy(new_node->eth_name, interfaceNames[i], strlen(interfaceNames[i]));
        new_node->isUP = true;
        new_node->next = NULL;
        add_entry_lbcast_LL(new_node); // Add entry to LHBT

        /* Path Cost Table (Table 2/8) - Contains non-MTP interfaces and their associated costs */
        struct path_cost_tuple *new_cost = (struct path_cost_tuple*) calloc(1, sizeof(struct path_cost_tuple));
        strncpy(new_cost->eth_name, interfaceNames[i], strlen(interfaceNames[i]));
        new_cost->path_cost = 1;
        new_cost->next = NULL;
        add_path_cost(new_cost); 			// Add entry to Path Cost Table
    }
    free(interfaceNames);

    runMTP(); // Implementation of Meshed Tree Protocol finite state machine

    return 0; // Graceful close of main function, this is the end of Meshed Tree Protocol operations
}


/*
 * Function: runMTP
 * ----------------------------
 *   The implementation of Meshed Tree Protocol finite state machine, the machine reacts via functions found in Feature Payload and MTP Send.
 *
 *   returns: NULL, this function should loop indefinitely while the MTP implementation is running.
 */
void runMTP()
{
    int sockCtrl = 0, sockData = 0, recv_len = 0;
    struct ether_header *eheader = NULL;
    struct sockaddr_ll src_addr;
    char **deletedVIDs, **deletedVIDs2;
    uint8_t recvBuffer[MAX_BUFFER_SIZE] = { '\0' }; 				 	// init all values to the NULL byte
    uint8_t clientBuffer[MAX_CLIENT_BUFFER_SIZE] = { '\0' }; 	// init all values to the NULL byte
    time_t time_advt_beg;                           					// Hello Timer start
    time_t time_advt_fin;                           					// Hello Timer end
    time_t time_current;

    deletedVIDs = (char**) calloc(MAX_VID_LIST * MAX_VID_LIST, sizeof(char));
    deletedVIDs2 = (char **) calloc(MAX_VID_LIST * MAX_VID_LIST, sizeof(char));

    /* Create raw sockets to listen to incoming MTP PDUs and end-node data traffic */
    if ((sockCtrl = socket(AF_PACKET, SOCK_RAW, htons (ETH_MTP_CTRL))) < 0) // Socket created, ETH_MTP_CTRL Ethertype is what it is listening for
    {
        perror("Error: MTP socket()");
        exit(1);
    }

    if ((sockData = socket(AF_PACKET, SOCK_RAW, htons (ETH_UNI_DATA))) < 0) // Socket created, ETH_UNI_DATA Ethertype is what it is listening for
    {
        perror("Error: MTP Unicast Data socket()");
        exit(1);
    }

    /* Not sure what to do with this right now... */
    if(signal(SIGINT, sig_handler) == SIG_ERR) // SIGINT = interrupt (Ctrl-c)
    {
        printf("\nCant Catch SIGINT");
    }

    /* Starts the clocks for periodic hello messages */
    time(&time_advt_beg);

    /* Startup behavior for MTS nodes */
    if(isPrimaryRoot) // If node is the root MTS
    {
        if(primaryRootVID != NULL) // Check if Root VID is provided through arguments
        {
            struct vid_addr_tuple *new_node = (struct vid_addr_tuple*) calloc(1, sizeof(struct vid_addr_tuple));

            // Create VID structure for user-defined root VID
            strncpy(new_node->vid_addr, primaryRootVID, strlen(primaryRootVID));
            strcpy(new_node->eth_name, "self");   	// own interface, so mark it as self, will be helpful while tracking own VIDs.
            new_node->last_updated = -1; 		        // -1 here because root ID should not be removed.
            new_node->port_status = PVID_PORT;
            new_node->next = NULL;
            new_node->isNew = true;
            new_node->path_cost = PATH_COST;
            new_node->membership = 1;

            system("echo This is the Primary root and adding it to the primary linked list >> MSTC.txt");
            printf("This is the Primary root and adding it to the primary linked list\n");
            add_entry_LL(new_node); // Add into Main VID Table (only VID that this node will receive)

            int treeNo = 1;
            treeStable = true;
            sendVIDUpdatePDU_ADD(ALL_HOST_PORTS, treeNo);

            printf("Sending out join msg for tree 2 \n");
            system("echo Sending out join msg for tree 2 >> MSTC.txt");
            treeNo = 2;
            floodNotificationPDU(MTP_TYPE_JOIN_MSG, treeNo);

        }

        else // Error out if root VID is not present
        {
            printf("Error: Provide Primary ROOT Switch ID [./mtpd <non MTS/root MTS> <ROOT MTS ID>]\n");
            exit(1);
        }
    }
    else if(isSecondaryRoot) // If node is the root MTS
    {
        if(secondaryRootVID != NULL) // Check if Root VID is provided through arguments
        {
            struct vid_addr_tuple *new_node = (struct vid_addr_tuple*) calloc(1, sizeof(struct vid_addr_tuple));

            // Create VID structure for user-defined root VID
            strncpy(new_node->vid_addr, secondaryRootVID, strlen(secondaryRootVID));
            strcpy(new_node->eth_name, "self");   	// own interface, so mark it as self, will be helpful while tracking own VIDs.
            new_node->last_updated = -1; 		        // -1 here because root ID should not be removed.
            new_node->port_status = PVID_PORT;
            new_node->next = NULL;
            new_node->isNew = true;
            new_node->path_cost = PATH_COST;
            new_node->membership = 2;

            system("echo This is the Secondary root and adding it to the secondary linked list >> MSTC.txt");
            printf("This is the Secondary root and adding it to the Secondary linked list\n");
            add_entry_LL2(new_node); // Add into Main VID Table (only VID that this node will receive)

            int treeNo = 2;
            treeStable = true;
            sendVIDUpdatePDU_ADD(ALL_HOST_PORTS, treeNo);

            printf("Sending out join msg for tree 1 \n");
            system("echo Sending out join msg for tree 1 >> MSTC.txt");
            treeNo = 1;
            floodNotificationPDU(MTP_TYPE_JOIN_MSG, treeNo);
        }

        else // Error out if root VID is not present
        {
            printf("Error: Provide Secondary ROOT Switch ID [./mtpd <non MTS/root MTS> <ROOT MTS ID>]\n");
            exit(1);
        }
    }

    else // If node is a non-root MTS
    {
        floodNotificationPDU(MTP_TYPE_JOIN_MSG, 1);
    }

    /* Start the Meshed Tree Protocol Finite State Machine (operation continues until user intervention) */
    while (true)
    {
        time(&time_advt_fin); // End the periodic timer for analysis
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
        checkForPrimaryLinkFailure(); // Check for any local link failures

        /* Mark the VIDs or HAT entries that have been affected by a failure */
        memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
        int numberOfDeletions = checkForPrimaryFailuresVID(deletedVIDs);
        bool hasCPVIDDeletions = checkForPrimaryFailuresCPVID();
        bool hasHATDeletions = checkForFailuresHAT();

        /* In the event of a VID deletion, alert all neighboring MTS' */
        if(numberOfDeletions > 0)
        {
            int treeNo = 1;
            sendVIDUpdatePDU_DEL(deletedVIDs, numberOfDeletions, treeNo);

            /* Check CPVID Table - I'm not sure if we really need this */
            int i = 0;
            for (; i < numberOfDeletions; i++)
            {
                delete_entry_cpvid_LL(deletedVIDs[i]); // Delete any CPVID that matches any of the deleted VIDs
            }

            struct vid_addr_tuple* parentMTPNode =  getInstance_vid_tbl_LL();
            if (parentMTPNode != NULL)
            {
                sendVIDUpdatePDU_ADD(parentMTPNode->eth_name, treeNo); // Announcing to my parent that I have a VID from him
            }
            printTablesPrimary(); // Print the updated tables to the console to view the changes
        }

        else if(hasCPVIDDeletions || hasHATDeletions)
        {
            printTablesPrimary(); // Print the updated tables to the console to view the changes
        }
        checkForSecondaryLinkFailure();
        memset(deletedVIDs2, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
        int numberOfDeletions2 = checkForSecondaryFailuresVID(deletedVIDs2);
        bool hasCPVIDDeletions2 = checkForSecondaryFailuresCPVID();

        /* In the event of a VID deletion, alert all neighboring MTS' */
        if(numberOfDeletions2 > 0)
        {
            int treeNo = 2;
            sendVIDUpdatePDU_DEL(deletedVIDs2, numberOfDeletions2, treeNo);

            /* Check CPVID Table - I'm not sure if we really need this */
            int i = 0;
            for (; i < numberOfDeletions2; i++)
            {
                delete_entry_cpvid_LL2(deletedVIDs2[i]); // Delete any CPVID that matches any of the deleted VIDs
            }

            struct vid_addr_tuple* parentMTPNode2 =  getInstance_vid_tbl_LL2();
            if (parentMTPNode2 != NULL)
            {
                sendVIDUpdatePDU_ADD(parentMTPNode2->eth_name, treeNo); // Announcing to my parent that I have a VID from him
            }
            printTablesSecondary(); // Print the updated tables to the console to view the changes
        }

        else if(hasCPVIDDeletions2 || hasHATDeletions)
        {
            printTablesSecondary(); // Print the updated tables to the console to view the changes
        }


        /* Take action whenever the Periodic Hello timer has elapsed (every 1 second) */
        if((double)(difftime(time_advt_fin, time_advt_beg) >= PERIODIC_HELLO_TIME))
        {
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
                floodNotificationPDU(MTP_TYPE_PERODIC_MSG, treeNo);
            }
            if(sendJoin){
                floodNotificationPDU(MTP_TYPE_JOIN_MSG, treeNo); // Send a Join MT_PDU out of all host interfaces to look for MTS neighbors
            }
            time(&time_advt_beg); // Reset the Periodic Hello Timer
        }

        /* Reset the buffer that holds incoming MT_PDUs traffic */
        int i = 0;
        for(i = 0; i < MAX_BUFFER_SIZE; i++)
        {
            recvBuffer[i] = '\0';
        }

        socklen_t addr_len = sizeof(src_addr);

        /* Receive an MT_PDU from a neighboring MTP-enabled Switch (recvBuffer = where the PDU is stored) */
        recv_len = recvfrom(sockCtrl, recvBuffer, MAX_BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr*) &src_addr, &addr_len);

        if (recv_len > 0) // As long as the incoming frame actually has data, analyze it
        {
            eheader = (struct ether_header*)recvBuffer; // Read the Ethernet II header from the MT_PDU

            char recvOnEtherPort[5]; // The interface where the MT_PDU was received on
            if_indextoname(src_addr.sll_ifindex, recvOnEtherPort); // Getting the readable name for the interface (ex: eth1)

            if ((strcmp(recvOnEtherPort, CONTROL_PORT)) == 0) // ignore frames that are from the GENI control interface.
            {
                continue;
            }

            else
            {
                delete_entry_lbcast_LL(recvOnEtherPort); // This is an MTP frame so, incase this port is in Local host broadcast table remove it.

                struct control_ports *new_node = (struct control_ports *) calloc(1, sizeof(struct control_ports));
                strncpy(new_node->eth_name, recvOnEtherPort, strlen(recvOnEtherPort));
                new_node->isUP = true;
                new_node->next = NULL;

                bool addedToControlTable = add_entry_control_table(new_node);

                if(addedToControlTable == false)
                {
                    free(new_node);
                }
            }
            int treeNo;
            //switch statement for 14th index of recvBuffer (frame),
            switch (recvBuffer[14])
            {
                /*
                 *MT_JOIN - this message is sent by a switch that hears  MT_ADVT messages on its ports and desires to join on one of the tree branches
         the *advertised in the MT_VIDs.
                */
                case MTP_TYPE_JOIN_MSG:
                {
                    char* ReceivedJoinMsg = "PDU [Join] Received";
                    MTPlog(ReceivedJoinMsg, recvOnEtherPort, MSG_RECV, MTP_TYPE_JOIN_MSG);

                    //if the VID table is empty, we can't be sending ADVT's out, we don't have anything to ADVT!
                    if(!isPrimary_VID_Table_Empty())
                    {
                        treeNo = 1;
                        sendVIDUpdatePDU_ADD(recvOnEtherPort, treeNo); // Payload destination will be from where the Join message orginated.
                    }
                    if(!isSecondary_VID_Table_Empty())
                    {
                        treeNo = 2;
                        sendVIDUpdatePDU_ADD(recvOnEtherPort, treeNo); // Payload destination will be from where the Join message orginated.
                    }
                }
                    break;

                    /*
                     *MT_HELLO – this message is a keep-alive indicator and issued periodically on all MTP_ports by a switch running the MTP.
             A root switch that sends *this message will carry the MT_VID of the root switch. A non-root switch that sends this message will carry
             all of its MT_VIDs.
                    */
                case MTP_TYPE_PERODIC_MSG:
                {
                    // Record MAC address, if not already present.
                    treeNo = (int) recvBuffer[15];
                    if (treeNo == 0 || treeNo == 1) {
                        struct ether_addr src_mac;
                        bool retMainVID, retCPVID;
                        int currentVIDTableSize;

                        memcpy(&src_mac, (struct ether_addr *) &eheader->ether_shost, sizeof(struct ether_addr));

                        retMainVID = update_hello_time_LL(&src_mac);
                        retCPVID = update_hello_time_cpvid_LL(&src_mac);
                        currentVIDTableSize = sizeOfPrimaryVIDTable();
                        update_hello_time_HAT(recvOnEtherPort);

                        char *helloMsg = "PDU [Hello] Received";
                        MTPlog(helloMsg, recvOnEtherPort, MSG_RECV, MTP_TYPE_PERODIC_MSG);

                        //if(retMainVID == false && retCPVID == false && !isRoot)
                        if (retMainVID == false && retCPVID == false && currentVIDTableSize < MAX_VID_ENTRIES &&
                            !isPrimaryRoot) //BIG ADD
                        {
                            floodNotificationPDU(MTP_TYPE_JOIN_MSG, 1);
                        }
                    }
                    if (treeNo == 0 || treeNo == 2) {
                        struct ether_addr src_mac2;
                        bool retMainVID2, retCPVID2;
                        int currentVIDTableSize2;

                        memcpy(&src_mac2, (struct ether_addr *) &eheader->ether_shost, sizeof(struct ether_addr));

                        retMainVID2 = update_hello_time_LL2(&src_mac2);
                        retCPVID2 = update_hello_time_cpvid_LL2(&src_mac2);
                        currentVIDTableSize2 = sizeOfSecondaryVIDTable();
                        update_hello_time_HAT(recvOnEtherPort);

                        char *helloMsg2 = "PDU [Hello] Received";
                        MTPlog(helloMsg2, recvOnEtherPort, MSG_RECV, MTP_TYPE_PERODIC_MSG);

                        //if(retMainVID == false && retCPVID == false && !isRoot)
                        if (retMainVID2 == false && retCPVID2 == false && currentVIDTableSize2 < MAX_VID_ENTRIES &&
                            !isSecondaryRoot) //BIG ADD
                        {
                            floodNotificationPDU(MTP_TYPE_JOIN_MSG,2);
                        }
                    }
                }
                    break;

                    /*
                     * ---MTP ADVERTISMENT MESSAGE TYPE 3---
                     *
                     *							MTP ADVERTISMENT MESSAGE
                     *  0                   1                   2
                     *	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
                     *	+-----------------------------------------------+
                     *	| Message Type  |   Operation   |   VID Count   |
                     *	+-----------------------------------------------+ =======
                     *	|   Path Cost   | VID Addr Len  |  VID Address  |       } These fields are repeated based on the VID Count value
                     *	+-----------------------------------------------+ =======
                     *
                     *  VID Address = variable length field
                     */
                case MTP_TYPE_VID_ADVT: {
                    /* I should put this in a function */
                    treeNo = (int) recvBuffer[17];
                    if(treeNo == 1){
                        char *mainVIDs[3] = {NULL, NULL, NULL};
                        if (sizeOfPrimaryVIDTable() >= 3) {
                            struct vid_addr_tuple *currentTable = getInstance_vid_tbl_LL();
                            int i = 0;
                            for (i = 0; i < 3; i++) {
                                mainVIDs[i] = currentTable->vid_addr;
                                currentTable = currentTable->next;
                            }
                        }

                        /* MTP ADVERTISMENT HEADER FIELD 1 (1 byte): OPERATION */
                        uint8_t operation = (uint8_t) recvBuffer[15];

                        if (operation ==
                            VID_ADD) // Operation option 00000001 (1): VIDs included in message should be considered for table addition
                        {
                            char *ReceivedAdvtAddMsg = "PDU [add] Received";
                            MTPlog(ReceivedAdvtAddMsg, recvOnEtherPort, MSG_RECV, MTP_TYPE_VID_ADVT);

                            /* MTP ADVERTISMENT HEADER FIELD 2 (1 byte): VID COUNT */
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16]; // Up to three VIDs can be included in a single VID Advertisment message

                            int tracker = 18; // tracks the byte number of the frame (VID count field was byte 16)
                            bool hasAdditions = false;
                            bool mainVIDTableChange = false;

                            while (numberVIDS != 0) // Checking the VIDs included in the Advertisment Message
                            {
                                /* MTP ADVERTISMENT HEADER FIELD 3 (1 byte): PATH COST */
                                uint8_t path_cost = (uint8_t) recvBuffer[tracker];
                                tracker = tracker + 1; // next byte

                                /* MTP ADVERTISMENT HEADER FIELD 4 (1 byte): VID ADDRESS LENGTH */
                                uint8_t vid_len = recvBuffer[tracker];
                                tracker = tracker + 1;

                                /* MTP ADVERTISMENT HEADER FIELD 5 (1 byte): VID ADDRESS */
                                char vid_addr[vid_len];
                                memset(vid_addr, '\0', vid_len);
                                strncpy(vid_addr, &recvBuffer[tracker], vid_len);
                                vid_addr[vid_len] = '\0';

                                tracker += vid_len; // byte after the length of the VID (including the "." inclusions)

                                int ret = isPrimaryChild(
                                        vid_addr); // Returns an int to tell the protocol whether or not the VID is the node's child or not

                                if (ret == 1) // VID is a child of a VID in the main VID table
                                {
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

                                        if (add_entry_cpvid_LL(
                                                new_cpvid)) // Add into child PVID table, if already there update it if any changes.
                                        {
                                            char CPVIDAdditionInfo[22 + vid_len];
                                            snprintf(CPVIDAdditionInfo, sizeof(CPVIDAdditionInfo),
                                                     "%s Added to CPVID Table", vid_addr);
                                            MTPlog(CPVIDAdditionInfo, recvOnEtherPort, MSG_INFO, MTP_TYPE_VID_ADVT);
                                        } else {
                                            free(new_cpvid); // If already there deallocate node memory
                                            // update CPVID time here
                                        }
                                    }
                                }

                                    // Add to Main VID Table, if not a child, make it PVID if there is no better path already in the table.
                                else if (ret == -1) {
                                    // Allocate memory and intialize
                                    struct vid_addr_tuple *new_node = (struct vid_addr_tuple *) calloc(1,
                                                                                                       sizeof(struct vid_addr_tuple));

                                    // Fill in data for new VID
                                    strncpy(new_node->vid_addr, vid_addr, strlen(vid_addr)); // VID Address
                                    strncpy(new_node->eth_name, recvOnEtherPort,
                                            strlen(recvOnEtherPort)); // Incoming Ethernet Port Name
                                    new_node->last_updated = time(0); // current timestamp
                                    new_node->port_status = PVID_PORT; // Port Type
                                    new_node->next = NULL; // Linked List Hook
                                    new_node->isNew = true; // Is New
                                    new_node->membership = 0;     // Intialize with '0', will find outpreference based on cost during add method.
                                    new_node->path_cost = (uint8_t) path_cost; // Path Cost
                                    memcpy(&new_node->mac, (struct ether_addr *) &eheader->ether_shost,
                                           sizeof(struct ether_addr)); // Source MAC address of VID ADVT

                                    // Add into VID Table, if addition success, update all other connected peers about the change.
                                    int mainVIDTracker = add_entry_LL(new_node);

                                    if (mainVIDTracker == 1 || mainVIDTracker == 2) // BIG ADD
                                    {
                                        if (mainVIDTracker == 1) {
                                            char VIDAdditionInfo[29 + vid_len];
                                            snprintf(VIDAdditionInfo, sizeof(VIDAdditionInfo),
                                                     "%s Added to Main VID Table [%d]", vid_addr, new_node->membership);
                                            MTPlog(VIDAdditionInfo, recvOnEtherPort, MSG_INFO, MTP_TYPE_VID_ADVT);

                                            hasAdditions = true;
                                        } else if (mainVIDTracker == 2) {
                                            char VIDAdditionInfo[31 + vid_len];
                                            snprintf(VIDAdditionInfo, sizeof(VIDAdditionInfo),
                                                     "%s Added to Backup VID Table [%d]", vid_addr, new_node->membership);
                                            MTPlog(VIDAdditionInfo, recvOnEtherPort, MSG_INFO, MTP_TYPE_VID_ADVT);
                                        }

                                        // If peer has VID derived from me earlier and has a change now.
                                        if (numberVIDS == (uint8_t) recvBuffer[16]) {
                                            // Check PVID used by peer is a derived PVID from me.
                                            delete_MACentry_cpvid_LL(&new_node->mac);
                                        }
                                    } else if (mainVIDTracker == 3) // BIG ADD
                                    {
                                        delete_entry_LL(new_node->vid_addr);
                                    } else {
                                        free(new_node);
                                    }
                                }
                                numberVIDS--;
                            }

                            if (sizeOfPrimaryVIDTable() > 3 && hasAdditions && mainVIDs[2] != NULL) {
                                memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
                                int numberToDelete = checkForPrimaryVIDTableChanges(mainVIDs, deletedVIDs);

                                if (numberToDelete > 0) {
                                    sendVIDUpdatePDU_DEL(deletedVIDs, numberToDelete, treeNo);
                                }
                            }

                            if (hasAdditions) {
                                sendVIDUpdatePDU_ADD(ALL_MTP_PORTS, treeNo);
                            }
                        } else if (operation == VID_DEL) {
                            char *ReceivedAdvtDelMsg = "PDU [Del] Received";
                            MTPlog(ReceivedAdvtDelMsg, recvOnEtherPort, MSG_RECV, MTP_TYPE_VID_DEL_ADVT);

                            // Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16];

                            // delete all local entries, get a list and send to others who derive from this VID.
                            memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);

                            uint8_t numberOfDeletions = numberVIDS;
                            bool hasDeletions = false;
                            bool needToCommDeletions = false;

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

                                hasDeletions = delete_entry_LL(
                                        deletedVIDs[i]); // Communicate the deletion of any VIDs YOU own
                                delete_entry_cpvid_LL(deletedVIDs[i]);

                                if (hasDeletions) {
                                    needToCommDeletions = true;
                                }

                                tracker += vid_len;
                                i++;
                            }

                            uint8_t *payload;
                            int payloadLen;

                            if (needToCommDeletions) // Only if we have deletions we will be advertising it to our MTP nearest neighbors
                            {
                                sendVIDUpdatePDU_DEL(deletedVIDs, numberOfDeletions, treeNo);

                                struct vid_addr_tuple *c1 = getInstance_vid_tbl_LL();
                                if (c1 != NULL) {
                                    sendVIDUpdatePDU_ADD(c1->eth_name, treeNo);
                                }
                            }
                        } else {
                            printf("Unknown VID Advertisment\n");
                        }
                        printTablesPrimary(); // This may be causing all of the extra prints that don't add or subtract anything
                    }
                    else{

                        char *mainVIDs[3] = {NULL, NULL, NULL};
                        if (sizeOfSecondaryVIDTable() >= 3) {
                            struct vid_addr_tuple *currentTable = getInstance_vid_tbl_LL2();
                            int i = 0;
                            for (i = 0; i < 3; i++) {
                                mainVIDs[i] = currentTable->vid_addr;
                                currentTable = currentTable->next;
                            }
                        }

                        /* MTP ADVERTISMENT HEADER FIELD 1 (1 byte): OPERATION */
                        uint8_t operation = (uint8_t) recvBuffer[15];

                        if (operation ==
                            VID_ADD) // Operation option 00000001 (1): VIDs included in message should be considered for table addition
                        {
                            char *ReceivedAdvtAddMsg = "PDU [add] Received";
                            MTPlog(ReceivedAdvtAddMsg, recvOnEtherPort, MSG_RECV, MTP_TYPE_VID_ADVT);

                            /* MTP ADVERTISMENT HEADER FIELD 2 (1 byte): VID COUNT */
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16]; // Up to three VIDs can be included in a single VID Advertisment message

                            int tracker = 18; // tracks the byte number of the frame (VID count field was byte 16)
                            bool hasAdditions = false;
                            bool mainVIDTableChange = false;

                            while (numberVIDS != 0) // Checking the VIDs included in the Advertisment Message
                            {
                                /* MTP ADVERTISMENT HEADER FIELD 3 (1 byte): PATH COST */
                                uint8_t path_cost = (uint8_t) recvBuffer[tracker];
                                tracker = tracker + 1; // next byte

                                /* MTP ADVERTISMENT HEADER FIELD 4 (1 byte): VID ADDRESS LENGTH */
                                uint8_t vid_len = recvBuffer[tracker];
                                tracker = tracker + 1;

                                /* MTP ADVERTISMENT HEADER FIELD 5 (1 byte): VID ADDRESS */
                                char vid_addr[vid_len];
                                memset(vid_addr, '\0', vid_len);
                                strncpy(vid_addr, &recvBuffer[tracker], vid_len);
                                vid_addr[vid_len] = '\0';

                                tracker += vid_len; // byte after the length of the VID (including the "." inclusions)

                                int ret = isSecondaryChild(
                                        vid_addr); // Returns an int to tell the protocol whether or not the VID is the node's child or not

                                if (ret == 1) // VID is a child of a VID in the main VID table
                                {
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

                                        if (add_entry_cpvid_LL2(
                                                new_cpvid)) // Add into child PVID table, if already there update it if any changes.
                                        {
                                            char CPVIDAdditionInfo[22 + vid_len];
                                            snprintf(CPVIDAdditionInfo, sizeof(CPVIDAdditionInfo),
                                                     "%s Added to CPVID Table", vid_addr);
                                            MTPlog(CPVIDAdditionInfo, recvOnEtherPort, MSG_INFO, MTP_TYPE_VID_ADVT);
                                        } else {
                                            free(new_cpvid); // If already there deallocate node memory
                                            // update CPVID time here
                                        }
                                    }
                                }

                                    // Add to Main VID Table, if not a child, make it PVID if there is no better path already in the table.
                                else if (ret == -1) {
                                    // Allocate memory and intialize
                                    struct vid_addr_tuple *new_node = (struct vid_addr_tuple *) calloc(1,
                                                                                                       sizeof(struct vid_addr_tuple));

                                    // Fill in data for new VID
                                    strncpy(new_node->vid_addr, vid_addr, strlen(vid_addr)); // VID Address
                                    strncpy(new_node->eth_name, recvOnEtherPort,
                                            strlen(recvOnEtherPort)); // Incoming Ethernet Port Name
                                    new_node->last_updated = time(0); // current timestamp
                                    new_node->port_status = PVID_PORT; // Port Type
                                    new_node->next = NULL; // Linked List Hook
                                    new_node->isNew = true; // Is New
                                    new_node->membership = 0;     // Intialize with '0', will find outpreference based on cost during add method.
                                    new_node->path_cost = (uint8_t) path_cost; // Path Cost
                                    memcpy(&new_node->mac, (struct ether_addr *) &eheader->ether_shost,
                                           sizeof(struct ether_addr)); // Source MAC address of VID ADVT

                                    // Add into VID Table, if addition success, update all other connected peers about the change.
                                    int mainVIDTracker = add_entry_LL2(new_node);

                                    if (mainVIDTracker == 1 || mainVIDTracker == 2) // BIG ADD
                                    {
                                        if (mainVIDTracker == 1) {
                                            char VIDAdditionInfo[29 + vid_len];
                                            snprintf(VIDAdditionInfo, sizeof(VIDAdditionInfo),
                                                     "%s Added to Main VID Table [%d]", vid_addr, new_node->membership);
                                            MTPlog(VIDAdditionInfo, recvOnEtherPort, MSG_INFO, MTP_TYPE_VID_ADVT);

                                            hasAdditions = true;
                                        } else if (mainVIDTracker == 2) {
                                            char VIDAdditionInfo[31 + vid_len];
                                            snprintf(VIDAdditionInfo, sizeof(VIDAdditionInfo),
                                                     "%s Added to Backup VID Table [%d]", vid_addr, new_node->membership);
                                            MTPlog(VIDAdditionInfo, recvOnEtherPort, MSG_INFO, MTP_TYPE_VID_ADVT);
                                        }

                                        // If peer has VID derived from me earlier and has a change now.
                                        if (numberVIDS == (uint8_t) recvBuffer[16]) {
                                            // Check PVID used by peer is a derived PVID from me.
                                            delete_MACentry_cpvid_LL2(&new_node->mac);
                                        }
                                    } else if (mainVIDTracker == 3) // BIG ADD
                                    {
                                        delete_entry_LL2(new_node->vid_addr);
                                    } else {
                                        free(new_node);
                                    }
                                }
                                numberVIDS--;
                            }

                            if (sizeOfSecondaryVIDTable() > 3 && hasAdditions && mainVIDs[2] != NULL) {
                                memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);
                                int numberToDelete = checkForSecondaryVIDTableChanges(mainVIDs, deletedVIDs);

                                if (numberToDelete > 0) {
                                    sendVIDUpdatePDU_DEL(deletedVIDs, numberToDelete, treeNo);
                                }
                            }

                            if (hasAdditions) {
                                sendVIDUpdatePDU_ADD(ALL_MTP_PORTS, treeNo);
                            }
                        } else if (operation == VID_DEL) {
                            char *ReceivedAdvtDelMsg = "PDU [Del] Received";
                            MTPlog(ReceivedAdvtDelMsg, recvOnEtherPort, MSG_RECV, MTP_TYPE_VID_DEL_ADVT);

                            // Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
                            uint8_t numberVIDS = (uint8_t) recvBuffer[16];

                            // delete all local entries, get a list and send to others who derive from this VID.
                            memset(deletedVIDs, '\0', sizeof(char) * MAX_VID_LIST * MAX_VID_LIST);

                            uint8_t numberOfDeletions = numberVIDS;
                            bool hasDeletions = false;
                            bool needToCommDeletions = false;

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

                                hasDeletions = delete_entry_LL2(
                                        deletedVIDs[i]); // Communicate the deletion of any VIDs YOU own
                                delete_entry_cpvid_LL2(deletedVIDs[i]);

                                if (hasDeletions) {
                                    needToCommDeletions = true;
                                }

                                tracker += vid_len;
                                i++;
                            }

                            uint8_t *payload;
                            int payloadLen;

                            if (needToCommDeletions) // Only if we have deletions we will be advertising it to our MTP nearest neighbors
                            {
                                sendVIDUpdatePDU_DEL(deletedVIDs, numberOfDeletions, treeNo);

                                struct vid_addr_tuple *c1 = getInstance_vid_tbl_LL2();
                                if (c1 != NULL) {
                                    sendVIDUpdatePDU_ADD(c1->eth_name, treeNo);
                                }
                            }
                        } else {
                            printf("Unknown VID Advertisment\n");
                        }
                        printTablesSecondary(); // This may be causing all of the extra prints that don't add or subtract anything
                    }
                }
                    break;

                case MTP_HAAdvt_TYPE:
                {
                    char* ReceivedHAADVTMsg = "PDU [HAADVT] Received";
                    MTPlog(ReceivedHAADVTMsg, recvOnEtherPort, MSG_RECV, MTP_HAAdvt_TYPE);

                    struct Host_Address_tuple *HAT = (struct Host_Address_tuple *) calloc(1, sizeof (struct Host_Address_tuple));
                    struct ether_addr *switchAddress = (struct ether_addr *) calloc(1, sizeof (struct ether_addr));

                    uint8_t mac_addr[6]; 		// This is used to differentiate the status of entry in the table.
                    int HATEntryType; 	 		// This flag is used to notify a change in the HAT and send the HAA messages accordingly.
                    bool hasUpdate = false;	// A flag used to notify the MTP FSM if an HAT entry has been updated / created

                    //print_HAAdvt_message_content(recvBuffer);

                    // <HOST MAC ADDRESS>

                    mac_addr[0] = recvBuffer[18];
                    mac_addr[1] = recvBuffer[19];
                    mac_addr[2] = recvBuffer[20];
                    mac_addr[3] = recvBuffer[21];
                    mac_addr[4] = recvBuffer[22];
                    mac_addr[5] = recvBuffer[23];
                    memcpy(&HAT->mac, (struct ether_addr *) mac_addr, sizeof(struct ether_addr));

                    // <SWITCH ID> (org switch where the HAAdvt came from)

                    switchAddress->ether_addr_octet[0] = (uint8_t) recvBuffer[24];
                    switchAddress->ether_addr_octet[1] = (uint8_t) recvBuffer[25];
                    switchAddress->ether_addr_octet[2] = (uint8_t) recvBuffer[26];
                    switchAddress->ether_addr_octet[3] = (uint8_t) recvBuffer[27];
                    switchAddress->ether_addr_octet[4] = (uint8_t) recvBuffer[28];
                    switchAddress->ether_addr_octet[5] = (uint8_t) recvBuffer[29];
                    HAT->switch_id = switchAddress; // Switch ID is assigned in the HAT entry from the previously read MAC address

                    // <SOURCE INTERFACE>
                    strncpy(HAT->eth_name, recvOnEtherPort, strlen(recvOnEtherPort));
                    HAT->path_cost = (uint8_t) recvBuffer[15] + find_path_cost(HAT->eth_name); // Adding path cost according to the interface received
                    HAT->time_current = time(0);
                    HAT->sequence_number = (uint8_t) recvBuffer[16]; // [needs fix?] seq # increments to send a triggered update on changed information
                    HAT->local = FALSE;
                    HAT->next = NULL;

                    /*
                        Flag to identify whether the host mac is in a HAT:
                                      0 =  Entry not present
                                      1 =  Entry with same host MAC address and switch ID
                                      2 =  Entry is there but switch ID is different
                        3 =  HAA received is from itself
                    */
                    HATEntryType = find_entry_HAT_LL(HAT);

                    /* [Type 3] The HAA sent by the switch has returned to itself, so discard it */
                    if(HATEntryType == LOCAL_HOST)
                    {
                        char HATSameInfo[28 + strlen(ether_ntoa(&HAT->mac))];
                        snprintf(HATSameInfo, sizeof(HATSameInfo), "%s HAADVT returned to source", ether_ntoa(&HAT->mac));
                        MTPlog(HATSameInfo, HAT->eth_name, MSG_INFO, MTP_HAAdvt_TYPE);

                        break;
                    }

                        /* [Type 0] The HAT does not contain this entry (added "else" to the if) */
                    else if(HATEntryType == NEW_ENTRY)
                    {
                        if(add_entry_HAT_LL(HAT))
                        {
                            hasUpdate = true;
                        }
                    }

                        /* [Type 2] HAT Entry present, but the switch ID is different */
                    else if(HATEntryType == DIFFERENT_SWITCH_ID)
                    {
                        hasUpdate = true;

                        char HATSwitchIDInfo[27 + strlen(ether_ntoa(&HAT->mac))];
                        snprintf(HATSwitchIDInfo, sizeof(HATSwitchIDInfo), "%s HAADVT new Switch ID [%d]", ether_ntoa(&HAT->mac), HAT->path_cost);
                        MTPlog(HATSwitchIDInfo, HAT->eth_name, MSG_INFO, HAT_ADDED);
                    }

                        /* [Type 1] Both the MAC entry and switch ID is the same */
                    else if(HATEntryType == ENTRY_PRESENT)
                    {
                        if(!find_port_arrival(HAT)) // If the port of the HAADVT is different from a previous HAT entry with the same addresses
                        {
                            if(compare_path_cost(HAT)) // If different port and HIGHER path cost, we need to update the entry in the BACKUP HAT
                            {
                                hasUpdate = update_bkp_HAT_LL(HAT);
                            }

                            else // If different port and LOWER path cost, we need to update the entry in the MAIN HAT
                            {
                                hasUpdate = update_main_HAT_LL(HAT);
                            }
                        }

                        else // If the HAADVT with the same addresses arrived on the same port as the current HAT entry
                        {
                            hasUpdate = check_cost99_sequence_no(HAT); // If port of arrival is same, comparing the sequence number
                        }
                    }

                    if(hasUpdate) // If there is any update in the tables, HAA messages have to be sent
                    {
                        sendHostUpdatePDU(HAT, recvOnEtherPort);
                        printTablesPrimary();
                        printTablesSecondary();

                    }
                }
                    break;

                default:
                    break;
            }
        }

        /* Receive data traffic */
        recv_len = recvfrom(sockData, clientBuffer, MAX_CLIENT_BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr*) &src_addr, &addr_len);

        if (recv_len > 0)
        {
            eheader = (struct ether_header*)clientBuffer;

            /*
            struct ether_addr srcAddress;
            memcpy(&srcAddress, (struct ether_addr*) &eheader->ether_shost, sizeof(struct ether_addr));

            struct ether_addr dstAddress;
            memcpy(&dstAddress, (struct ether_addr*) &eheader->ether_dhost, sizeof(struct ether_addr));

            printf("\nrecv length: %d\n", recv_len);
            printf("source MAC transferred: %s\n", ether_ntoa(&srcAddress));
            printf("source MAC: %s\n", ether_ntoa((struct ether_addr *)&eheader->ether_shost));
            printf("destination MAC: %s\n", ether_ntoa((struct ether_addr *)&eheader->ether_dhost));
            printf("destination MAC transferred: %s\n", ether_ntoa(&dstAddress));

            printf("\nbuffer after recvOnEtherPort (first 30 bytes):\n");
            int p = 0;
            for(p = 0; p < 30; p++)
            {
                printf("%02x", clientBuffer[p]);
            }
            printf("\n");
            */

            char recvOnEtherPort[5];
            if_indextoname(src_addr.sll_ifindex, recvOnEtherPort); // THIS IS THE HAT ISSUE RIGHT HERE (maybe)

            if((strcmp(recvOnEtherPort, CONTROL_PORT)) == 0) // Ignore frames that are from the control interface
            {
                continue;
            }

            /* BROADCAST FRAME - flood frame on appropriate interfaces */
            if(strncmp(ether_ntoa((struct ether_addr *)&eheader->ether_dhost), "ff:ff:ff:ff:ff:ff", 17) == 0)
            {
                //printf("Received broadcast frame\n");
                floodFrame(clientBuffer, recv_len, recvOnEtherPort, treetoUse);
            }

                /* MULTICAST FRAME -  disregard frame */
            else if(strncmp(ether_ntoa((struct ether_addr *)&eheader->ether_dhost), "01:00:5e:00:00:01", 9) == 0)
            {
                markMulticastFrame(recvOnEtherPort);
            }

                /* UNICAST FRAME - switch frame on apporpriate interface */
            else
            {
                switchUnicastFrame(clientBuffer, recv_len, recvOnEtherPort, treetoUse);
            }
        } // End receive of data message
    } // End of MTP Finite State Machine (infinite while loop)
} // End of runMTP() function


void floodFrame(uint8_t *recvBuffer, int recv_len, char *recvOnEtherPort, int treetoUse)
{
    /* SEND TO HOST PORTS */
    struct local_bcast_tuple* current =  getInstance_lbcast_LL();
    for (; current != NULL; current = current->next)
    {
        if (strcmp(current->eth_name, recvOnEtherPort) != 0) // port should NOT be the same from where it received frame
        {
            dataSend(current->eth_name, recvBuffer, recv_len);
        }
    }

    /* SEND TO CHILD MTS' PORTS */
    struct child_pvid_tuple* cpt;
    if(treetoUse ==1) {
        cpt = getInstance_cpvid_LL();
    }
    else{
        cpt = getInstance_cpvid_LL2();
    }
    for (; cpt != NULL; cpt = cpt->next)
    {
        if (strcmp(cpt->child_port, recvOnEtherPort) != 0)
        {
            dataSend(cpt->child_port, recvBuffer, recv_len);
        }
    }

    /* SEND TO PARENT MTS ON THE PVID PORT (if it didn't originate from that location) */
    if(treetoUse ==1) {
        if (!isPrimaryRoot) {
            struct vid_addr_tuple *vid_t = getInstance_vid_tbl_LL();

            if (strcmp(vid_t->eth_name, recvOnEtherPort) != 0) {
                dataSend(vid_t->eth_name, recvBuffer, recv_len);
            }
        }
    }
    else{
        if (!isSecondaryRoot) {
            struct vid_addr_tuple *vid_t = getInstance_vid_tbl_LL2();

            if (strcmp(vid_t->eth_name, recvOnEtherPort) != 0) {
                dataSend(vid_t->eth_name, recvBuffer, recv_len);
            }
        }
    }

    return;
}


void markMulticastFrame(char *recvOnEtherPort)
{
    /*
    Multicast address range is from 01:00:5e:00:00:00 to 01:00:5e:7f:ff:ff.
    Do not handle frames addressed to multicast destinations.
    */
    printf("Received on port %s a multicast frame \n", recvOnEtherPort);

    return;
}


/*
  This function is split into two tasks:
    1. Switching the unicast frame or flooding it if there is no HAT entry for the destination
    2. Updating the HAT with the source of the traffic (sender MAC address)
		recvBuffer usually has a *, removed for testing
*/
void switchUnicastFrame(uint8_t *recvBuffer, int recv_len, char *recvOnEtherPort, int treetoUse)
{
    struct ether_header *eheader = NULL;
    eheader = (struct ether_header*) recvBuffer;

    /*
    printf("\nbuffer at switchUnicastFrame (first 30 bytes):\n");
    int i = 0;
    for(i = 0; i < 30; i++)
    {
        printf("%02x", recvBuffer[i]);
    }
    printf("\n");
    */

    bool switchedFrame = attemptUnicastSwitching(recvBuffer, eheader, recv_len);
    if(switchedFrame == TRUE)
    {
        //printf("Received unicast frame\n");
    }

    else
    {
        //printf("Received unicast frame (Desination not in HAT, broadcasting)\n");
        floodFrame(recvBuffer, recv_len, recvOnEtherPort, treetoUse);
    }

    /* Checking whether the sender host details are present in the HAT, if not, it is added and advertised to the other MTS' */
    if(isHostPort(recvOnEtherPort))
    {
        struct Host_Address_tuple* HAT = (struct Host_Address_tuple *) calloc(1, sizeof (struct Host_Address_tuple));
        struct ether_addr* switch_id = (struct ether_addr *) calloc(1, sizeof (struct ether_addr *));

        strncpy(HAT->eth_name, recvOnEtherPort, strlen(recvOnEtherPort));
        HAT->switch_id = get_switchid();
        HAT->path_cost = find_path_cost(HAT->eth_name); // path cost according to the interface received.
        HAT->sequence_number = SEQUENCE_NUMBER; // starts with sequence number 0 as this is the local port
        memcpy(&HAT->mac, (struct ether_addr*) &eheader->ether_shost, sizeof(struct ether_addr));
        HAT->local = TRUE; // This is the key to this section, we are looking for locally connected clients to the MTS
        HAT->time_current = time(0);
        HAT->next = NULL;

        /* Checks if the host address is already present in the table, if not, this address is added as a local host in the table */
        if(!find_entry_HAT_LL(HAT))
        {
            if(add_entry_HAT_LL(HAT)) // Adding the host address entry to the host address table
            {
                sendHostUpdatePDU(HAT, recvOnEtherPort);
                printTablesPrimary();
                printTablesSecondary();

            }

            else
            {
                //printf("Entry cannot be added to HAT (path cost = %d)\n", HAT->path_cost);
                free(HAT);
                free(switch_id);
            }
        }

        else
        {
            free(HAT);
            free(switch_id);
        }
    }

    return;
}


// get active interfaces on the node.
int getActiveInterfaces(char **ptr)
{
    // find all interfaces on the node.
    int indexLen = 0;

    //structures that describe the network interfaces of the local system
    struct ifaddrs *ifaddr, *ifa;

    /*The getifaddrs() function creates a linked list of structures describing the network interfaces of the local system,
     *and stores the address of the first item of the list in *ifa.  The list consists of ifaddrs structures
     */
    if(getifaddrs(&ifaddr))
    {
        perror("Error: getifaddrs Failed\n");
        exit(0);
    }

    // loop through the list, the last part moves the list to the next struct (interface) as long as there still is one (!NULL)
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {

        //if the interfaces has no address, continue
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        //family reaches into ifa_addr, which is a pointer to a struct sockaddr variable that describes the address family
        int family;
        family = ifa->ifa_addr->sa_family;

        // populate interface names, if interface is in the AF_INET family, UP, and if ethernet interface doesn't belong to control interface and Loopback interface.
        if (family == AF_INET && (strncmp(ifa->ifa_name, "lo", 2) != 0) && (ifa->ifa_flags & IFF_UP) != 0)
        {
            char networkIP[NI_MAXHOST];

            //IPv4 AF_INET sockets, grab the IP address from the general ifaddrs struct
            struct sockaddr_in *ipaddr = ((struct sockaddr_in*) ifa->ifa_addr);

            //Converts a numeric address into a text string suitable for presentation (family, source, destination, size)
            inet_ntop(AF_INET, &(ipaddr->sin_addr), networkIP, INET_ADDRSTRLEN);

            //manually checking the output of the binary-to-text output of inet_ntop
            //printf("IP address being checked for getActiveInt: %s\n", networkIP);

            //compares first three bytes to determine if its control interface from GENI (172.16.0.0/12)
            if (strncmp(networkIP, CTRL_IP, 3) == 0)
            {
                // skip, as it is control interface.
                //printf("we are skipping over the control addr %s\n", networkIP);
                continue;
            }

            //take the argument given to the function, pointer to memory the size of all the interfaces, give it as the dest arg in strncpy.
            //+1 for the null terminator, which is given in ifa_name by the ifaddrs structure
            ptr[indexLen] = (char*) calloc(strlen(ifa->ifa_name) + 1, sizeof(char));
            strncpy(ptr[indexLen], ifa->ifa_name, strlen(ifa->ifa_name));
            indexLen++;
        }
    }
    freeifaddrs(ifaddr);
    //printf("size of indexlen: %d\n", indexLen);
    return indexLen;
}


void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        printf("received SIGINT\n");
        exit(1);
    }
}
