#ifndef FT_PYL_H
#define FT_PYL_H

#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
#include <errno.h>            // errno, perror()
#include <ifaddrs.h>
#include <netinet/ether.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>

#define VID_ADDR_LEN           40				// This may be too small, where did 20 come from?
#define ETH_ADDR_LEN           5        // As in the name of the interface, not a 802.3 MAC Address

#define MAX_BUFFER_SIZE        	1024
#define MAX_CLIENT_BUFFER_SIZE	1500 		// Allows for the standard MTU of 802.3 Ethernet

#define MAX_INTERFACES         8

#define NO_PORTS							 "None"
#define ALL_MTP_PORTS          "eth-1"  // Added to know when you want to loop all interfaces
#define ALL_HOST_PORTS         "eth-2"
#define CONTROL_PORT           "eth0"

/* Meshed Tree Protocol PDU Types */
#define MTP_TYPE_JOIN_MSG       1
#define MTP_TYPE_PERODIC_MSG   	2
#define MTP_TYPE_VID_ADVT	      3
#define MTP_HAAdvt_TYPE         4

#define MAX_MAIN_VID_TBL_PATHS  3
#define MAX_VID_ENTRIES 				6

#define VID_ADD			            1
#define VID_DEL			            2

#define PVID_PORT 		          1
#define NON_PVID_PORT		        2

#define PATH_COST		            0
#define SEQUENCE_NUMBER         0

#define PERIODIC_HELLO_TIME	    1.0 // The elapsed time to update all MTS neighbors, was 2.0

/* Type of ports */
#define MTP_PORT                1
#define HOST_PORT               2

/* Logging Direction Types */
#define MSG_INFO								0 // Logged for the implementation start time, failures, etc
#define MSG_SENT								1 // Logged for when a PDU is transmitted out of a given interface
#define MSG_RECV								2 // Logged for when a PDU is received from a given interface

/* Additional Logging Types (beyond standard PDU types) */
#define MTP_TYPE_VID_DEL_ADVT	  5 // Not used for actual transmission/reception, use 3 (VID_ADVT)
#define VID_ADDED								6 // Logged for when a MT_VID is added to a table
#define VID_REMOVED							7 // Logged for when a MT_VID is deleted from a table
#define FAILURE_ALERT						8 // Logged for when an interface or node fails
#define HAT_ADDED								9
#define HAT_REMOVED							10

#define TRUE                    1
#define FALSE                   0

/* Type of return messages from Host Address Table Find Funciton */
#define NEW_ENTRY								0
#define ENTRY_PRESENT						1
#define DIFFERENT_SWITCH_ID 		2
#define LOCAL_HOST       				3


/* Container for VID Table */
struct vid_addr_tuple
{
    char eth_name[ETH_ADDR_LEN]; 	// Port of Acquisition
    char vid_addr[VID_ADDR_LEN];	// VID Address.
    time_t last_updated;      	// last updated time
    int port_status;		// Port Type i.e. PVID, MTP
    int isNew;
    struct vid_addr_tuple *next;
    uint8_t path_cost;		// VID path cost.
    struct ether_addr mac;
    int membership;			// Membership PRIMARY, SECONDARY, TERTIARY
};

/* Container for Child PVID Table */
struct child_pvid_tuple
{
    char child_port[ETH_ADDR_LEN];  // Port name through which child is connected through
    char vid_addr[VID_ADDR_LEN];    // The VID of child, we might not need it, temporarily storing.
    struct ether_addr mac;
    struct child_pvid_tuple *next;
    time_t last_updated;        // last updated time
};

/* Container for End-Node Interfaces Table */
struct local_bcast_tuple
{
    char eth_name[ETH_ADDR_LEN]; 		// Host port Name
    int isUP;               				// true [1] (if UP) or false [2] (if DOWN)
    struct local_bcast_tuple *next;
};

/* Container for MTP Interfaces Table */
struct control_ports
{
    char eth_name[ETH_ADDR_LEN]; 		// Control port Name
    int isUP;               				// true [1] (if UP) or false [2] (if DOWN)
    struct control_ports *next;
};

/* Container for Host Addresses Table */
struct Host_Address_tuple
{
    char eth_name[ETH_ADDR_LEN];   // Port of access for host
    struct ether_addr *switch_id;
    struct ether_addr mac;         // MAC address of host
    uint8_t path_cost;             // Path cost to reach host.
    uint8_t sequence_number;
    time_t time_current;
    bool local;                    // If the host is local this flag will be set to true - else false
    struct Host_Address_tuple *next;
};

/* Container for Path Costs Table */
struct path_cost_tuple
{
    char eth_name[ETH_ADDR_LEN];
    uint8_t path_cost; // Port Name
    struct path_cost_tuple *next;
};

/* Function Prototypes for payloads */
int  build_JOIN_MSG_PAYLOAD(uint8_t *);
int  build_PERIODIC_MSG_PAYLOAD(uint8_t *, int);
int  build_VID_ADVT_PAYLOAD(uint8_t *, char *, int);
int  build_VID_CHANGE_PAYLOAD(uint8_t *, char *, char **, int, int); // params - payload, interfacename, deleted VIDS, number of deleted VIDs
bool isMain_VID_Table_Empty();
int isChild(char *);
bool isPrimary_VID_Table_Empty();
bool isSecondary_VID_Table_Empty();
int isPrimaryChild(char *);
int isSecondaryChild(char *);

/* Function Prototypes for Primary VID Table Linked List */
int add_entry_LL(struct vid_addr_tuple *);
bool find_entry_LL(struct vid_addr_tuple *);
void printPrimaryVIDTable();
bool update_hello_time_LL(struct ether_addr *);
struct vid_addr_tuple* getInstance_vid_tbl_LL();
bool delete_entry_LL(char *);

/* Function Prototypes for Primary BKP VID Table Linked List */
void printPrimaryBackupVIDTable();

/* Function Prototypes for Primary CPVID table information */
bool add_entry_cpvid_LL(struct child_pvid_tuple *);
bool find_entry_cpvid_LL(struct child_pvid_tuple *);
void printPrimaryChildPVIDTable();
struct child_pvid_tuple* getInstance_cpvid_LL();
bool delete_entry_cpvid_LL(char *);
bool delete_MACentry_cpvid_LL(struct ether_addr *);
bool update_hello_time_cpvid_LL(struct ether_addr *);
bool update_entry_cpvid_LL(struct child_pvid_tuple *);

/* Function Prototypes for Secondary  VID Table Linked List */
int add_entry_LL2(struct vid_addr_tuple *);
bool find_entry_LL2(struct vid_addr_tuple *);
void printSecondaryVIDTable();
bool update_hello_time_LL2(struct ether_addr *);
struct vid_addr_tuple* getInstance_vid_tbl_LL2();
bool delete_entry_LL2(char *);

/* Function Prototypes for Secondary BKP VID Table Linked List */
void printSecondaryBackupVIDTable();

/* Function Prototypes for Secondary CPVID table information */
bool add_entry_cpvid_LL2(struct child_pvid_tuple *);
bool find_entry_cpvid_LL2(struct child_pvid_tuple *);
void printSecondaryChildPVIDTable();
struct child_pvid_tuple* getInstance_cpvid_LL2();
bool delete_entry_cpvid_LL2(char *);
bool delete_MACentry_cpvid_LL2(struct ether_addr *);
bool update_hello_time_cpvid_LL2(struct ether_addr *);
bool update_entry_cpvid_LL2(struct child_pvid_tuple *);

/* Function Prototypes for Local host broadcast information */
bool add_entry_lbcast_LL(struct local_bcast_tuple *);
bool find_entry_lbcast_LL(struct local_bcast_tuple *);
void printLocalHostBroadcastTable();
bool delete_entry_lbcast_LL(char *port);
struct local_bcast_tuple* getInstance_lbcast_LL();

/* Function prototypes for the Host Address Table  */
bool add_entry_HAT_LL(struct Host_Address_tuple *);
int find_entry_HAT_LL(struct Host_Address_tuple *);
void printMainHostAddressTable();
int build_HAAdvt_message(uint8_t *, struct ether_addr, uint8_t, uint8_t, struct ether_addr *);
void print_HAAdvt_message_content(uint8_t *);
int find_port_arrival(struct Host_Address_tuple *);
bool add_entry_bkp_HAT_LL(struct Host_Address_tuple *);
void printBackupHostAddressTable();
bool compare_path_cost(struct Host_Address_tuple *);
bool update_main_HAT_LL(struct Host_Address_tuple *);
bool update_bkp_HAT_LL(struct Host_Address_tuple *);
bool check_cost99_sequence_no(struct Host_Address_tuple *);
void delete_entry_HAT_LL(struct Host_Address_tuple *);
void delete_entry_bkp_HAT_LL(struct Host_Address_tuple *);
bool attemptUnicastSwitching(uint8_t *, struct ether_header *, int );
bool check_local_port(int ,struct ether_header *);
void update_hello_time_HAT(char *);
struct Host_Address_tuple* getInstance_HAT_LL();
struct Host_Address_tuple* getInstance_HAT_bkp_LL();
void add_path_cost(struct path_cost_tuple *);
uint8_t find_path_cost(char *);
bool find_path_cost_LL(struct path_cost_tuple *);
void printPathCostTable();

/* function prototypes for control ports table */
struct control_ports* getInstance_control_LL();
bool add_entry_control_table(struct control_ports *);
bool find_entry_control_table(struct control_ports  *);
void printControlPortsTable();
struct ether_addr* get_switchid();
void delete_control_port_LL(char *);

/* check Failures */
int checkForPrimaryFailuresVID(char **);
bool checkForPrimaryFailuresCPVID();

int checkForSecondaryFailuresVID(char **);
bool checkForSecondaryFailuresCPVID();
bool checkForFailuresHAT();

/* Peter Functional Prototypes */
int sizeOfPrimaryVIDTable();
int checkForPrimaryVIDTableChanges(char **, char **);
void printTablesPrimary();
void checkForPrimaryLinkFailure();

int sizeOfSecondaryVIDTable();
int checkForSecondaryVIDTableChanges(char **, char **);
void printTablesSecondary();
void checkForSecondaryLinkFailure();

void floodNotificationPDU(int, int);
void sendVIDUpdatePDU_ADD(char*, int);
void sendVIDUpdatePDU_DEL(char**, int, int);
bool checkInterfaceIsActive(char*);

/* In progress prototypes */
void MTPlog(char*, char*, int, int);
int getTimeStamp(char*, uint, struct timespec*);
void sendHostUpdatePDU(struct Host_Address_tuple *, char*);
bool isControlPort(char*); // TODAY CHANGES
bool isHostPort(char*);
bool isIdenticalMACAddress(struct ether_addr *, struct ether_addr *);

#endif /* FT_PYL_H */
