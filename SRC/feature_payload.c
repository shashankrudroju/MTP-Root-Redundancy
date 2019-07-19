/* INCLUDED LIBRARIES */
#include "mtp_send.h"
#include "feature_payload.h"

/* GLOBAL VARIABLES */
struct vid_addr_tuple *primary_vid_tbl_head = NULL;
struct child_pvid_tuple *primary_cpvid_tbl_head = NULL;
struct vid_addr_tuple *secondary_vid_tbl_head = NULL;
struct child_pvid_tuple *secondary_cpvid_tbl_head = NULL;
struct local_bcast_tuple *local_bcast_head = NULL;
struct Host_Address_tuple *HAT_head = NULL;
struct Host_Address_tuple *HAT_bkp_head = NULL;
struct control_ports *control_ports_head = NULL;
struct path_cost_tuple *path_cost_head = NULL;


/************************ TABLE LOCATION FUNCTIONS ************************/
/**
 *              Get Instance of Main VID Table.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              struct primary_vid_tbl_head* - reference of main vid table.
**/
struct vid_addr_tuple* getInstance_vid_tbl_LL()
{
    return primary_vid_tbl_head;
}

struct vid_addr_tuple* getInstance_vid_tbl_LL2()
{
    return secondary_vid_tbl_head;
}

/**
 *    Get instance of child of PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/
struct child_pvid_tuple* getInstance_cpvid_LL()
{
    return primary_cpvid_tbl_head;
}

struct child_pvid_tuple* getInstance_cpvid_LL2()
{
    return secondary_cpvid_tbl_head;
}

/**
 *    Return instance of local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    struct local_bcast_tuple* - Returns reference to local host broadcast table.
**/
struct local_bcast_tuple* getInstance_lbcast_LL()
{
    return local_bcast_head;
}


struct Host_Address_tuple* getInstance_HAT_LL()
{
    return HAT_head;
}


struct Host_Address_tuple* getInstance_HAT_bkp_LL()
{
    return HAT_bkp_head;
}


struct control_ports* getInstance_control_LL()
{
    return control_ports_head;
}


/************************ TABLE ADDITION FUNCTIONS ************************/
/**
 * 		Add into the VID Table, new VID's are added based on the path cost.
 *     		VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 * 		@return
 *   -1			  Failure to add/ Already exists.
 * 		1 			Successful Addition, addition in first 3 entries of Main VID Table
 * 		2   	  Successful Addition, addition after first 3 entries of Main VID Table (Backup VID Table)
 *		3       Successful Addtion, but more than the limit of 6 VID's total (3 in main, 3 in backup)
**/
int add_entry_LL(struct vid_addr_tuple *node)
{
    struct vid_addr_tuple *current = primary_vid_tbl_head;
    bool VIDpruning = false;

    int tracker = 0;
    if (!find_entry_LL(node))
    {
        if (primary_vid_tbl_head == NULL)
        {
            node->membership = 1;
            primary_vid_tbl_head = node;

            //tracker++;
            tracker = 1;
        }

        else
        {
            struct vid_addr_tuple *previous = NULL;
            int mship = 0;

            // place in accordance with cost, lowest to highest.
            //while(current!=NULL && (current->path_cost < node->path_cost))
            while(current!=NULL && (current->path_cost < node->path_cost || current->path_cost == node->path_cost)) //BIG ADD
            {
                previous = current;
                mship = current->membership;
                current = current->next;
                //tracker += 1;
            }

            // if new node has lowest cost.
            if (previous == NULL)
            {
                node->next = primary_vid_tbl_head;
                node->membership = 1;
                primary_vid_tbl_head = node;
                //tracker += 1;
                tracker = 1;
            }

            else
            {
                previous->next = node;
                node->next = current;
                node->membership = (mship + 1);

                if(node->membership <= 3)
                {
                    tracker = 1;
                }

                else if(node->membership > 3 && node->membership <= 6)
                {
                    tracker = 2;
                }

                else if(node->membership > 6) // BIG ADD
                {
                    tracker = 3;
                }
            }

            // Increment the membership IDs of other VID's
            while (current != NULL)
            {
                current->membership++;
                current = current->next;
            }

            if(sizeOfPrimaryVIDTable() > MAX_VID_ENTRIES && tracker != 3) // BIG ADD
            {
                for (current = primary_vid_tbl_head; current != NULL; current = current->next)
                {
                    if(current->membership > MAX_VID_ENTRIES)
                    {
                        delete_entry_LL(current->vid_addr);
                    }
                }
            }
        }
        return tracker;
    }

    return -1;
}

int add_entry_LL2(struct vid_addr_tuple *node)
{
    struct vid_addr_tuple *current = secondary_vid_tbl_head;
    bool VIDpruning = false;

    int tracker = 0;
    if (!find_entry_LL2(node))
    {
        if (secondary_vid_tbl_head == NULL)
        {
            node->membership = 1;
            secondary_vid_tbl_head = node;

            //tracker++;
            tracker = 1;
        }

        else
        {
            struct vid_addr_tuple *previous = NULL;
            int mship = 0;

            // place in accordance with cost, lowest to highest.
            //while(current!=NULL && (current->path_cost < node->path_cost))
            while(current!=NULL && (current->path_cost < node->path_cost || current->path_cost == node->path_cost)) //BIG ADD
            {
                previous = current;
                mship = current->membership;
                current = current->next;
                //tracker += 1;
            }

            // if new node has lowest cost.
            if (previous == NULL)
            {
                node->next = secondary_vid_tbl_head;
                node->membership = 1;
                secondary_vid_tbl_head = node;
                //tracker += 1;
                tracker = 1;
            }

            else
            {
                previous->next = node;
                node->next = current;
                node->membership = (mship + 1);

                if(node->membership <= 3)
                {
                    tracker = 1;
                }

                else if(node->membership > 3 && node->membership <= 6)
                {
                    tracker = 2;
                }

                else if(node->membership > 6) // BIG ADD
                {
                    tracker = 3;
                }
            }

            // Increment the membership IDs of other VID's
            while (current != NULL)
            {
                current->membership++;
                current = current->next;
            }

            if(sizeOfPrimaryVIDTable() > MAX_VID_ENTRIES && tracker != 3) // BIG ADD
            {
                for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
                {
                    if(current->membership > MAX_VID_ENTRIES)
                    {
                        delete_entry_LL2(current->vid_addr);
                    }
                }
            }
        }
        return tracker;
    }

    return -1;
}
/*
int add_entry_LL(struct vid_addr_tuple *node)
{
	struct vid_addr_tuple *current = primary_vid_tbl_head;
	bool VIDpruning = false;

	int tracker = 0;
	if (!find_entry_LL(node))
	{
		if (primary_vid_tbl_head == NULL)
		{
			node->membership = 1;
			primary_vid_tbl_head = node;

			//tracker++;
			tracker = 1;
		}

		else
		{
			struct vid_addr_tuple *previous = NULL;
			int mship = 0;

			// place in accordance with cost, lowest to highest.
			//while(current!=NULL && (current->path_cost < node->path_cost))
      while(current!=NULL && (current->path_cost < node->path_cost || current->path_cost == node->path_cost)) //BIG ADD
			{
				previous = current;
				mship = current->membership;
				current = current->next;
				//tracker += 1;
			}

			// if new node has lowest cost.
			if (previous == NULL)
			{
				node->next = primary_vid_tbl_head;
				node->membership = 1;
				primary_vid_tbl_head = node;
				//tracker += 1;
				tracker = 1;
			}

			else
			{
				previous->next = node;
				node->next = current;
				node->membership = (mship + 1);

				if(node->membership <= 3)
				{
					tracker = 1;
				}

				else if(node->membership > 3 && node->membership <= 6)
				{
					tracker = 2;
				}
			}

			// Increment the membership IDs of other VID's
			while (current != NULL)
			{
				current->membership++;
				current = current->next;
			}

			if(sizeOfPrimaryVIDTable() > MAX_VID_ENTRIES)
			{
				for (current = primary_vid_tbl_head; current != NULL; current = current->next)
				{
					if(current->membership > MAX_VID_ENTRIES)
					{
						delete_entry_LL(current->vid_addr);
					}
				}
			}
		}
		return tracker;
	}

	return -1;
}
*/

/**
 *    Add into the Child PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/already exists.
**/
bool add_entry_cpvid_LL(struct child_pvid_tuple *node)
{
    bool newAddition = false;

    if(primary_cpvid_tbl_head != NULL)
    {
        bool updatedNode = update_entry_cpvid_LL(node);

        if(!updatedNode && !find_entry_cpvid_LL(node))
        {
            node->next = primary_cpvid_tbl_head;
            primary_cpvid_tbl_head = node;
            newAddition = true;
        }
    }

    else
    {
        primary_cpvid_tbl_head = node;
        newAddition = true;
    }

    return newAddition;
}

bool add_entry_cpvid_LL2(struct child_pvid_tuple *node)
{
    bool newAddition = false;

    if(secondary_cpvid_tbl_head != NULL)
    {
        bool updatedNode = update_entry_cpvid_LL2(node);

        if(!updatedNode && !find_entry_cpvid_LL2(node))
        {
            node->next = secondary_cpvid_tbl_head;
            secondary_cpvid_tbl_head = node;
            newAddition = true;
        }
    }

    else
    {
        secondary_cpvid_tbl_head = node;
        newAddition = true;
    }

    return newAddition;
}

/**
 *    Add in Table.
 *    Local Host broadcast Table, Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/
bool add_entry_lbcast_LL(struct local_bcast_tuple *node)
{
    bool newAddition = false;

    if (local_bcast_head == NULL)
    {
        local_bcast_head = node;
        newAddition = true;
    }

    else
    {
        if (!find_entry_lbcast_LL(node))
        {
            node->next = local_bcast_head;
            local_bcast_head = node;
            newAddition = true;
        }
        newAddition = false;
    }

    return newAddition;
}


bool add_entry_control_table(struct control_ports *node)
{
    if (control_ports_head == NULL)
    {
        control_ports_head = node;
    }

    else
    {
        if(!find_entry_control_table(node))
        {
            node->next = control_ports_head;
            control_ports_head = node;
        }

        else
        {
            return false;
        }
    }
    return true;
}


// This function adds interface path cost entry to the LL
void add_path_cost(struct path_cost_tuple *node)
{
    if(path_cost_head == NULL)
    {
        path_cost_head = node;
    }

    else
    {
        if(!find_path_cost_LL(node))
        {
            node->next = path_cost_head;
            path_cost_head = node;
        }
    }
}


bool add_entry_HAT_LL(struct Host_Address_tuple *HAT) // added back to everything is true
{
    bool newAddition = true;

    if(HAT->path_cost < 99)
    {
        if (HAT_head == NULL)
        {
            HAT_head = HAT;
        }

        else
        {
            HAT->next = HAT_head;
            HAT_head = HAT;  // The entry is being added
        }

        newAddition = true;

        char HATAdditionInfo[35 + strlen(ether_ntoa(&HAT->mac))];
        snprintf(HATAdditionInfo, sizeof(HATAdditionInfo), "%s Added to Main Host Table [%d] {%d}", ether_ntoa(&HAT->mac), HAT->path_cost, HAT->local);
        MTPlog(HATAdditionInfo, HAT->eth_name, MSG_INFO, HAT_ADDED);
    }

    return newAddition;
}


// This function populates the HAT backup table
bool add_entry_bkp_HAT_LL(struct Host_Address_tuple *HAT)
{
    if (HAT->path_cost < 99)
    {
        if (HAT_bkp_head == NULL)
        {
            HAT_bkp_head = HAT;  // update local variable with the passed variable - first time this will be true
        }

        else
        {
            HAT->next = HAT_bkp_head;
            HAT_bkp_head = HAT;
        }

        char HATBckupAdditionInfo[37 + strlen(ether_ntoa(&HAT->mac))];
        snprintf(HATBckupAdditionInfo, sizeof(HATBckupAdditionInfo), "%s Added to Backup Host Table [%d] {%d}", ether_ntoa(&HAT->mac), HAT->path_cost, HAT->local);
        MTPlog(HATBckupAdditionInfo, HAT->eth_name, MSG_INFO, HAT_ADDED);
    }

    return TRUE;
}


/************************ TABLE DELETION FUNCTIONS ************************/
/**
 *              Delete Entries in the vid_table using vid as a reference.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              true - if deletion in main VID table occurs (should it do the backup table?)
 *              false - if deletion fails.
**/
bool delete_entry_LL(char *vid_to_delete)
{
    struct vid_addr_tuple *current = primary_vid_tbl_head;
    struct vid_addr_tuple *previous = NULL;
    bool hasDeletionsInMainVID = false; // top 3
    int hasDeletions = false;
    int tracker = 0;

    while (current != NULL)
    {
        tracker += 1;
        if (strncmp(vid_to_delete, current->vid_addr, strlen(vid_to_delete)) == 0)
        {
            if(current->membership < 7) //trying to get rid of extra stuff that clogs up logs
            {
                char VIDDeletionInfo[32 + strlen(current->vid_addr) + strlen(vid_to_delete)];
                snprintf(VIDDeletionInfo, sizeof(VIDDeletionInfo), "%s {%d} Deleted From VID Table [%s]", current->vid_addr, current->membership, vid_to_delete);
                MTPlog(VIDDeletionInfo, current->eth_name, MSG_INFO, VID_REMOVED);
            }

            struct vid_addr_tuple *temp = current;

            if (previous == NULL)
            {
                primary_vid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            if (tracker > 0 && tracker <= 3)
            {
                hasDeletionsInMainVID  = true;
            }

            hasDeletions = true;
            free(temp);
            continue;
        }
        previous = current;
        current = current->next;
    }

    // fix any wrong membership values.
    if (hasDeletions)
    {
        int membership = 1;
        for (current = primary_vid_tbl_head; current != NULL; current = current->next)
        {
            current->membership = membership;
            membership++;
        }
    }

    return hasDeletionsInMainVID;
}

bool delete_entry_LL2(char *vid_to_delete)
{
    struct vid_addr_tuple *current = secondary_vid_tbl_head;
    struct vid_addr_tuple *previous = NULL;
    bool hasDeletionsInMainVID = false; // top 3
    int hasDeletions = false;
    int tracker = 0;

    while (current != NULL)
    {
        tracker += 1;
        if (strncmp(vid_to_delete, current->vid_addr, strlen(vid_to_delete)) == 0)
        {
            if(current->membership < 7) //trying to get rid of extra stuff that clogs up logs
            {
                char VIDDeletionInfo[32 + strlen(current->vid_addr) + strlen(vid_to_delete)];
                snprintf(VIDDeletionInfo, sizeof(VIDDeletionInfo), "%s {%d} Deleted From VID Table [%s]", current->vid_addr, current->membership, vid_to_delete);
                MTPlog(VIDDeletionInfo, current->eth_name, MSG_INFO, VID_REMOVED);
            }

            struct vid_addr_tuple *temp = current;

            if (previous == NULL)
            {
                secondary_vid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            if (tracker > 0 && tracker <= 3)
            {
                hasDeletionsInMainVID  = true;
            }

            hasDeletions = true;
            free(temp);
            continue;
        }
        previous = current;
        current = current->next;
    }

    // fix any wrong membership values.
    if (hasDeletions)
    {
        int membership = 1;
        for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
        {
            current->membership = membership;
            membership++;
        }
    }

    return hasDeletionsInMainVID;
}


/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *
 *    @input
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/
bool delete_entry_cpvid_LL(char *cpvid_to_be_deleted)
{
    struct child_pvid_tuple *current = primary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    bool hasDeletions = false;

    while (current != NULL)
    {
        if (strncmp(cpvid_to_be_deleted, current->vid_addr, strlen(cpvid_to_be_deleted)) == 0)
        {
            char CPVIDDeletionInfo[29 + strlen(current->vid_addr) + strlen(cpvid_to_be_deleted)];
            snprintf(CPVIDDeletionInfo, sizeof(CPVIDDeletionInfo), "%s Deleted From CPVID Table [%s]", current->vid_addr, cpvid_to_be_deleted);
            MTPlog(CPVIDDeletionInfo, current->child_port, MSG_INFO, VID_REMOVED);

            struct child_pvid_tuple *temp = current;

            if (previous == NULL)
            {
                primary_cpvid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(temp);
            hasDeletions = true;
            continue;
        }

        previous = current;
        current = current->next;
    }
    return hasDeletions;
}

bool delete_entry_cpvid_LL2(char *cpvid_to_be_deleted)
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    bool hasDeletions = false;

    while (current != NULL)
    {
        if (strncmp(cpvid_to_be_deleted, current->vid_addr, strlen(cpvid_to_be_deleted)) == 0)
        {
            char CPVIDDeletionInfo[29 + strlen(current->vid_addr) + strlen(cpvid_to_be_deleted)];
            snprintf(CPVIDDeletionInfo, sizeof(CPVIDDeletionInfo), "%s Deleted From CPVID Table [%s]", current->vid_addr, cpvid_to_be_deleted);
            MTPlog(CPVIDDeletionInfo, current->child_port, MSG_INFO, VID_REMOVED);

            struct child_pvid_tuple *temp = current;

            if (previous == NULL)
            {
                secondary_cpvid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(temp);
            hasDeletions = true;
            continue;
        }

        previous = current;
        current = current->next;
    }
    return hasDeletions;
}

/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *
 *    @input
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/
bool delete_MACentry_cpvid_LL(struct ether_addr *mac)
{
    struct child_pvid_tuple *current = primary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    bool hasDeletions = false;

    while (current != NULL)
    {
        if(memcmp(mac, &current->mac, sizeof(struct ether_addr)) == 0)
        {
            struct child_pvid_tuple *temp = current;

            if (previous == NULL)
            {
                primary_cpvid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(temp);
            hasDeletions = true;
            continue;
        }
        previous = current;
        current = current->next;
    }

    return hasDeletions;
}

bool delete_MACentry_cpvid_LL2(struct ether_addr *mac)
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    bool hasDeletions = false;

    while (current != NULL)
    {
        if(memcmp(mac, &current->mac, sizeof(struct ether_addr)) == 0)
        {
            struct child_pvid_tuple *temp = current;

            if (previous == NULL)
            {
                secondary_cpvid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(temp);
            hasDeletions = true;
            continue;
        }
        previous = current;
        current = current->next;
    }

    return hasDeletions;
}
/**
 *    Delete a port from local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true  - if deletion successful.
 *    false - if deletion not successful.
**/
bool delete_entry_lbcast_LL(char *port)
{
    struct local_bcast_tuple *current = local_bcast_head;
    struct local_bcast_tuple *previous = NULL;
    bool isPortDeleted = false;

    while (current != NULL)
    {
        if (strcmp(current->eth_name, port) == 0)
        {
            if (current == local_bcast_head)
            {
                local_bcast_head = current->next;
                free(current);
                current = local_bcast_head;
                previous = NULL;
                continue;
            }

            else
            {
                struct local_bcast_tuple *temp = current;
                current = current->next;
                previous->next = current;
                free(temp);
                continue;
            }

            isPortDeleted = true;
        }
        previous = current;
        current = current->next;
    }
    return isPortDeleted;
}


void delete_entry_HAT_LL(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current = HAT_head;
    struct Host_Address_tuple *previous = NULL;

    while (current != NULL)
    {
        if (memcmp(&node->mac, &current->mac, sizeof(struct ether_addr)) == 0)
        {
            char HATDelInfo[35 + strlen(ether_ntoa(&node->mac))];
            snprintf(HATDelInfo, sizeof(HATDelInfo), "%s Deleted from Main Host Table [%d]", ether_ntoa(&node->mac), node->path_cost);
            MTPlog(HATDelInfo, node->eth_name, MSG_INFO, HAT_REMOVED);

            if (current == HAT_head)
            {
                HAT_head = current->next;
                free(current);
                current = HAT_head;
                previous = NULL;
                continue;
            }

            else
            {
                struct Host_Address_tuple *temp = current;
                current = current->next;
                previous->next = current;
                free(temp);
                continue;
            }
        }
        previous = current;
        current = current->next;
    }
}


void delete_entry_bkp_HAT_LL(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *bkpcurrent = HAT_bkp_head;
    struct Host_Address_tuple *previous = NULL;

    while (bkpcurrent != NULL)
    {
        if (memcmp(&node->mac, &bkpcurrent->mac, sizeof(struct ether_addr)) == 0)
        {
            char HATDelBckupInfo[37 + strlen(ether_ntoa(&node->mac))];
            snprintf(HATDelBckupInfo, sizeof(HATDelBckupInfo), "%s Deleted From Backup Host Table [%d]", ether_ntoa(&node->mac), node->path_cost);
            MTPlog(HATDelBckupInfo, node->eth_name, MSG_INFO, HAT_REMOVED);

            if (bkpcurrent == HAT_bkp_head)
            {
                HAT_bkp_head = bkpcurrent->next;
                free(bkpcurrent);
                bkpcurrent = HAT_bkp_head;
                previous = NULL;
                continue;
            }

            else
            {
                struct Host_Address_tuple *temp = bkpcurrent;
                bkpcurrent = bkpcurrent->next;
                previous->next = bkpcurrent;
                free(temp);
                continue;
            }
        }
        previous = bkpcurrent;
        bkpcurrent = bkpcurrent->next;
    }
}


void delete_control_port_LL(char *port)
{
    struct control_ports *current = control_ports_head;
    struct control_ports *previous = NULL;

    while (current != NULL)
    {
        if (strcmp(current->eth_name, port) == 0)
        {
            if (current == control_ports_head)
            {
                control_ports_head = current->next;
                free(current);
                current = control_ports_head;
                previous = NULL;
                continue;
            }

            else
            {
                struct control_ports *temp = current;
                current = current->next;
                previous->next = current;
                free(temp);
                continue;
            }
        }
        previous = current;
        current = current->next;
    }
}


/************************ TABLE SEARCH FUNCTIONS ************************/
/**
 *		Check if the VID entry is already present in the table.
 *      VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		true			Element Found.
 *		false			Element Not Found.
**/
bool find_entry_LL(struct vid_addr_tuple *node)
{
    if (primary_vid_tbl_head != NULL)
    {
        struct vid_addr_tuple *current = primary_vid_tbl_head;
        while (current != NULL)
        {
            if (strcmp(current->vid_addr, node->vid_addr) == 0)
            {
                current->last_updated = time(0);
                return true;
            }
            current = current->next;
        }
    }
    return false;
}

bool find_entry_LL2(struct vid_addr_tuple *node)
{
    if (secondary_vid_tbl_head != NULL)
    {
        struct vid_addr_tuple *current = secondary_vid_tbl_head;
        while (current != NULL)
        {
            if (strcmp(current->vid_addr, node->vid_addr) == 0)
            {
                current->last_updated = time(0);
                return true;
            }
            current = current->next;
        }
    }
    return false;
}

/**
 *    Check if the VID entry is already present in the table.
 *    Child PVID Table,  Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *    @return
 *    true      Element Found.
 *    false     Element Not Found.
**/
bool find_entry_cpvid_LL(struct child_pvid_tuple *node)
{
    struct child_pvid_tuple *current = primary_cpvid_tbl_head;

    if (current != NULL)
    {
        while (current != NULL)
        {
            if (strcmp(current->vid_addr, node->vid_addr) == 0)
            {
                return true;
            }
            current = current->next;
        }
    }
    return false;
}

bool find_entry_cpvid_LL2(struct child_pvid_tuple *node)
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;

    if (current != NULL)
    {
        while (current != NULL)
        {
            if (strcmp(current->vid_addr, node->vid_addr) == 0)
            {
                return true;
            }
            current = current->next;
        }
    }
    return false;
}

/**
 *    Find entries in the local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      If element is found.
 *    false     If element is not found.
**/
bool find_entry_lbcast_LL(struct local_bcast_tuple *node)
{
    struct local_bcast_tuple *current = local_bcast_head;

    if (current != NULL)
    {
        while (current != NULL)
        {
            if (strcmp(current->eth_name, node->eth_name) == 0)
            {
                return true;
            }
            current = current->next;
        }
    }

    return false;
}


int find_entry_HAT_LL(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current = HAT_head;
    if(current != NULL)
    {
        while(current != NULL)
        {
            if(isIdenticalMACAddress(&current->mac, &node->mac)) // If the node MAC Address entries are the same
            {
                current->time_current = time(0); // Time stamp is updated

                if(isIdenticalMACAddress(current->switch_id, node->switch_id)) // If the Switch ID entries are the same
                {
                    if(current->local == TRUE)
                    {
                        return LOCAL_HOST; // The same frame returned to the source since local variable is true
                    }

                    else
                    {
                        return ENTRY_PRESENT; // There is an entry with same host MAC address and switch ID
                    }
                }

                else
                {
                    strncpy(current->eth_name, node->eth_name, ETH_ADDR_LEN);
                    current->sequence_number++;
                    node->sequence_number++;
                    current->switch_id = node->switch_id;
                    current->local = FALSE;
                    current->path_cost = node->path_cost;

                    return DIFFERENT_SWITCH_ID; // If switch id is different, sequence number incremented and value returned
                }
            }
            current = current->next;
        }
    }

    return NEW_ENTRY; // If the entry is not present
}

/*
TODAY CHANGES
int find_entry_HAT_LL(struct Host_Address_tuple *node)
{
	struct Host_Address_tuple *current = HAT_head;
  uint8_t mac_addr [6];
  uint8_t byte_number;
  int i;

  if(current != NULL)
	{
    while(current != NULL)
		{
			for(byte_number = 0; byte_number < 6; byte_number++)
      {
				if(current->mac.ether_addr_octet[byte_number] != node->mac.ether_addr_octet[byte_number]) // compares the MAC values
				{
					break;
				}
      }

      if(byte_number == 6) // MAC address is same, hence comparing switch ID
			{
				current->time_current = time(0); // Time stamp is updated

        for(i = 0; i < 6; i++)
				{
					if (node->switch_id->ether_addr_octet[i] != current->switch_id->ether_addr_octet[i]) // Switch ID is different
					{
          	strncpy(current->eth_name, node->eth_name, ETH_ADDR_LEN);
          	current->sequence_number++;
          	node->sequence_number++;
          	current->switch_id = node->switch_id;
          	current->local = FALSE;
          	current->path_cost = node->path_cost;

          	return DIFFERENT_SWITCH_ID; // If switch id is different, sequence number incremented and value returned
          }
        }

        if (current->local == TRUE) // Same frame returned to the source since local variable is true
				{
          return LOCAL_HOST;
        }

        return ENTRY_PRESENT; // If there is an entry with same host MAC address and switch ID
      }
      current = current->next;
    }
  }

  return NEW_ENTRY; // If the entry is not present
}
*/

/**
 *    Find entries in the local host broadcast Table.
 *    is being used for finding entries in control ports table also
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      If element is found.
 *    false     If element is not found.
**/
bool find_entry_control_table(struct control_ports  *node)
{
    struct control_ports *current = control_ports_head;

    if (current != NULL)
    {
        while (current != NULL)
        {
            if (strcmp(current->eth_name, node->eth_name) == 0)
            {
                return true;
            }
            current = current->next;
        }
    }

    return false;
}


uint8_t find_path_cost(char *ptr) // This function receives the interface and returns the path cost of it.
{
    struct path_cost_tuple *current;
    for(current = path_cost_head; current != NULL; current = current->next)
    {
        if(!(strcmp(current->eth_name, ptr)))
        {
            return current->path_cost;
        }
    }

    return 100; // Value set for dummy. This case doesn't occur.
}


bool find_path_cost_LL(struct path_cost_tuple  *node)
{
    struct path_cost_tuple  *current = path_cost_head;
    if(current != NULL)
    {
        while(current != NULL)
        {
            if (strcmp(current->eth_name, node->eth_name) == 0)
            {
                return true;
            }
            current = current->next;
        }
    }

    return false;
}


// Function returns the value for port of arrival check from HAT list.
int find_port_arrival(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current;
    int flag = 0;

    for (current = HAT_head; current != NULL; current = current->next)
    {
        if((!(strcmp(current->eth_name, node->eth_name))) && (memcmp(&node->mac, &current->mac, sizeof(struct ether_addr)) == 0))
        {
            flag = 1;
            break;
        }
    }

    return flag;
}


/************************ TABLE/ENTRY ANALYSIS FUNCTIONS ************************/
/*
 *   isChild() - This method checks if the input VID param is child of any VID in Main
 * 		 table or backup vid table.
 *   @input
 *   param1 -   char *       vid
 *
 *   @return
 *   3  - child VID that would create a loop/does not fit the protocol standards
 *   2  - duplicate
 *   1 	- if is a child of one of VID's in main VID Table.
 *   0 	- if VID is parent ID of one of the VID's in the main VID Table.
 *  -1  - if VID is not child of any of the VID's in the main VID Table.
 *
 */
int isPrimaryChild(char *vid)
{
    //checking only the Main VID table.
    if(primary_vid_tbl_head != NULL)
    {
        struct vid_addr_tuple *current = primary_vid_tbl_head;
        int lenInputVID = strlen(vid);

        while(current != NULL)
        {
            int lenCurrentVID = strlen(current->vid_addr);

            // This check is if we get a parent ID
            if(lenCurrentVID > lenInputVID && strncmp(current->vid_addr, vid, lenInputVID) == 0)
            {
                lenCurrentVID = lenInputVID;
                //printf("\nwe hit the 0");
                return 0;
            }

                // if length is same and are similar then its a duplicate no need to add.
            else if((lenInputVID == lenCurrentVID) && strncmp(vid, current->vid_addr, lenCurrentVID) == 0)
            {
                //printf("\nwe hit the 2");
                return 2;
            }

            else if(strncmp(vid, current->vid_addr, lenCurrentVID) == 0)
            {
                //printf("\nlength of current VID: %d\nlength of input VID: %d", lenCurrentVID, lenInputVID);
                if(lenInputVID > (lenCurrentVID + 2))
                {
                    //printf("\nwe hit the 3");
                    return 3;
                }

                //printf("\nwe hit the 1");
                return 1;
            }
            current = current->next;
        }
    }
    //printf("\nwe hit the -1");
    return -1;
}

int isSecondaryChild(char *vid)
{
    //checking only the Main VID table.
    if(secondary_vid_tbl_head != NULL)
    {
        struct vid_addr_tuple *current = secondary_vid_tbl_head;
        int lenInputVID = strlen(vid);

        while(current != NULL)
        {
            int lenCurrentVID = strlen(current->vid_addr);

            // This check is if we get a parent ID
            if(lenCurrentVID > lenInputVID && strncmp(current->vid_addr, vid, lenInputVID) == 0)
            {
                lenCurrentVID = lenInputVID;
                //printf("\nwe hit the 0");
                return 0;
            }

                // if length is same and are similar then its a duplicate no need to add.
            else if((lenInputVID == lenCurrentVID) && strncmp(vid, current->vid_addr, lenCurrentVID) == 0)
            {
                //printf("\nwe hit the 2");
                return 2;
            }

            else if(strncmp(vid, current->vid_addr, lenCurrentVID) == 0)
            {
                //printf("\nlength of current VID: %d\nlength of input VID: %d", lenCurrentVID, lenInputVID);
                if(lenInputVID > (lenCurrentVID + 2))
                {
                    //printf("\nwe hit the 3");
                    return 3;
                }

                //printf("\nwe hit the 1");
                return 1;
            }
            current = current->next;
        }
    }
    //printf("\nwe hit the -1");
    return -1;
}

/*
 *   isMain_VID_Table_Empty -     Check if Main VID Table is empty.
 *
 *   @input
 *   no params
 *
 *   @return
 *   true  	- if Main VID Table is empty.
 *   false 	- if Main VID Table is not null.
 */

bool isPrimary_VID_Table_Empty()
{
    if (primary_vid_tbl_head)
    {
        return false;
    }

    return true;
}

bool isSecondary_VID_Table_Empty()
{
    if (secondary_vid_tbl_head)
    {
        return false;
    }

    return true;
}

bool isControlPort(char *interface)
{
    bool isAControlPort = false;

    struct control_ports *currentMTPPort;
    for (currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
    {
        // if (!(strcmp(current->eth_name, ptr))) TODAY CHANGES
        if(strcmp(currentMTPPort->eth_name, interface) == 0)
        {
            isAControlPort = true;
        }
    }

    return isAControlPort;
}


// TODAY CHANGES
bool isHostPort(char *interface)
{
    bool isAHostPort = false;

    struct local_bcast_tuple *currentHostPort;
    for(currentHostPort = local_bcast_head; currentHostPort != NULL; currentHostPort = currentHostPort->next)
    {
        if(strcmp(currentHostPort->eth_name, interface) == 0)
        {
            isAHostPort = true;
        }
    }

    return isAHostPort;
}


int sizeOfPrimaryVIDTable()
{
    int size = 0;
    struct vid_addr_tuple *current;

    for (current = primary_vid_tbl_head; current != NULL; current = current->next)
    {
        size++;
    }

    return size;
}

int sizeOfSecondaryVIDTable()
{
    int size = 0;
    struct vid_addr_tuple *current;

    for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
    {
        size++;
    }

    return size;
}


int checkForPrimaryVIDTableChanges(char **VIDs, char **deletedVIDs)
{
    struct vid_addr_tuple *current;
    int numberOfFailures = 0;
    bool noVIDMatches = true;
    int i = 0;

    for(i = 0; i < 3; i++)
    {
        //when there are more than 1 deletion, this needs to be reset..
        noVIDMatches = true;

        for(current = primary_vid_tbl_head; current->membership < 4; current = current->next)
        {
            //printf("comparing %s and %s \n", VIDs[i], current->vid_addr);
            if ((strcmp(VIDs[i], current->vid_addr)) == 0)
            {
                noVIDMatches = false;
                break;
            }
        }

        if(noVIDMatches)
        {
            deletedVIDs[numberOfFailures] = (char*)calloc(strlen(VIDs[i]), sizeof(char));
            strncpy(deletedVIDs[numberOfFailures], VIDs[i], strlen(VIDs[i]));

            //printf("New VID (from deletedVIDs): %s\n", deletedVIDs[numberOfFailures]);
            numberOfFailures++;
        }
    }

    return numberOfFailures;
}

int checkForSecondaryVIDTableChanges(char **VIDs, char **deletedVIDs)
{
    struct vid_addr_tuple *current;
    int numberOfFailures = 0;
    bool noVIDMatches = true;
    int i = 0;

    for(i = 0; i < 3; i++)
    {
        //when there are more than 1 deletion, this needs to be reset..
        noVIDMatches = true;

        for(current = secondary_vid_tbl_head; current->membership < 4; current = current->next)
        {
            //printf("comparing %s and %s \n", VIDs[i], current->vid_addr);
            if ((strcmp(VIDs[i], current->vid_addr)) == 0)
            {
                noVIDMatches = false;
                break;
            }
        }

        if(noVIDMatches)
        {
            deletedVIDs[numberOfFailures] = (char*)calloc(strlen(VIDs[i]), sizeof(char));
            strncpy(deletedVIDs[numberOfFailures], VIDs[i], strlen(VIDs[i]));

            //printf("New VID (from deletedVIDs): %s\n", deletedVIDs[numberOfFailures]);
            numberOfFailures++;
        }
    }

    return numberOfFailures;
}

// Function checks for lower path cost or greater
bool compare_path_cost (struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current = HAT_head;
    uint8_t mac_addr [6];
    uint8_t byte_number;

    if(current != NULL)
    {
        while(current != NULL)
        {
            for(byte_number = 0; byte_number < 6; byte_number++)
            {
                if(current->mac.ether_addr_octet[byte_number] != node->mac.ether_addr_octet[byte_number])
                {
                    break;
                }
            }

            if(byte_number == 6)
            {
                if(node->path_cost >= current->path_cost)
                {
                    return true; //returns 1 if new node has greater cost than current node, else returns 0.
                }
            }
            current = current->next;
        }
    }

    return false;
}


/************************ ENTRY TIMESTAMP UPDATE FUNCTIONS ************************/
/**
 *              Update timestamp for a MAC address on both VID Table and backup table.
 *     		VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              bool
**/
bool update_hello_time_LL(struct ether_addr *mac)
{
    struct vid_addr_tuple *current;
    bool hasUpdates = false;

    for (current = primary_vid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, mac, sizeof (struct ether_addr)) == 0)
        {
            current->last_updated = time(0);
            hasUpdates = true;
        }
    }

    return hasUpdates;
}

bool update_hello_time_LL2(struct ether_addr *mac)
{
    struct vid_addr_tuple *current;
    bool hasUpdates = false;

    for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, mac, sizeof (struct ether_addr)) == 0)
        {
            current->last_updated = time(0);
            hasUpdates = true;
        }
    }

    return hasUpdates;
}

/**
 *              Update timestamp for a MAC address on both CPVID Table and backup table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *primary_cpvid_tbl_head
 *
 *              @return
 *              void
**/
bool update_hello_time_cpvid_LL(struct ether_addr *mac)
{
    struct child_pvid_tuple *current;
    bool isUpdated = false;

    for (current = primary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0)
        {
            current->last_updated = time(0);
            isUpdated = true;
        }
    }

    return isUpdated;
}

bool update_hello_time_cpvid_LL2(struct ether_addr *mac)
{
    struct child_pvid_tuple *current;
    bool isUpdated = false;

    for (current = secondary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0)
        {
            current->last_updated = time(0);
            isUpdated = true;
        }
    }

    return isUpdated;
}


void update_hello_time_HAT(char *port)
{
    struct Host_Address_tuple *current;
    struct Host_Address_tuple *bkpcurrent;

    for (current = HAT_head; current != NULL; current = current->next)
    {
        if(!(strcmp(current->eth_name,port)))
        {
            current->time_current = time(0);
        }
    }

    for (bkpcurrent = HAT_bkp_head; bkpcurrent != NULL; bkpcurrent = bkpcurrent->next)
    {
        if (!(strcmp(bkpcurrent->eth_name,port)))
        {
            bkpcurrent->time_current = time(0);
        }
    }
}


/************************ ENTRY UPDATE FUNCTIONS ************************/
/**
 *              Update entry for a MAC address on both CPVID Table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *primary_cpvid_tbl_head
 *
 *              @return
 *              void
**/
bool update_entry_cpvid_LL(struct child_pvid_tuple *node)
{
    struct child_pvid_tuple *current;

    for(current = primary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, &node->mac, sizeof(struct ether_addr)) == 0)
        {
            memset(current->vid_addr, '\0', VID_ADDR_LEN);
            strncpy(current->vid_addr, node->vid_addr, strlen(node->vid_addr));
            return true;
        }
    }

    return false;
}

bool update_entry_cpvid_LL2(struct child_pvid_tuple *node)
{
    struct child_pvid_tuple *current;

    for(current = secondary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, &node->mac, sizeof(struct ether_addr)) == 0)
        {
            memset(current->vid_addr, '\0', VID_ADDR_LEN);
            strncpy(current->vid_addr, node->vid_addr, strlen(node->vid_addr));
            return true;
        }
    }

    return false;
}

bool update_main_HAT_LL(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current = HAT_head;
    struct Host_Address_tuple *previous = NULL;
    bool hasUpdate = false;

    while(current != NULL)
    {
        if(memcmp(&node->mac, &current->mac, sizeof(struct ether_addr)) == 0)
        {
            bool movedToBackupHAT = update_bkp_HAT_LL(current); // If entry is found in HAT table, it has to be checked with backup table

            struct Host_Address_tuple *temp = current;

            if(previous == NULL)
            {
                HAT_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;

            if(!movedToBackupHAT)
            {
                free(temp);
            }

            char HATupdateInfo[30 + strlen(ether_ntoa(&node->mac))];
            snprintf(HATupdateInfo, sizeof(HATupdateInfo), "%s updated main path cost to %d", ether_ntoa(&node->mac), node->path_cost);
            MTPlog(HATupdateInfo, node->eth_name, MSG_INFO, HAT_ADDED);

            add_entry_HAT_LL(node);
            hasUpdate = true;
            continue;
        }
        previous = current;
        current = current->next;
    }

    return hasUpdate;
}


bool update_bkp_HAT_LL(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current = HAT_bkp_head;
    struct Host_Address_tuple *previous = NULL;
    bool hasUpdate = false;
    int flag = 0;

    if (current != NULL)
    {
        while (current != NULL)
        {
            if(memcmp(&node->mac, &current->mac, sizeof(struct ether_addr)) == 0)
            {
                flag = 1;

                if(current->path_cost > node->path_cost) // Updating due to low path cost
                {
                    struct Host_Address_tuple *temp = current;

                    if (previous == NULL)
                    {
                        HAT_bkp_head = current->next;
                    }

                    else
                    {
                        previous->next = current->next;
                    }

                    current = current->next;
                    free(temp); // Deleting the entry for new addition.

                    char HATBackupUpdateInfo[30 + strlen(ether_ntoa(&node->mac))];
                    snprintf(HATBackupUpdateInfo, sizeof(HATBackupUpdateInfo), "%s updated backup path cost to %d", ether_ntoa(&node->mac), node->path_cost);
                    MTPlog(HATBackupUpdateInfo, node->eth_name, MSG_INFO, HAT_ADDED);

                    add_entry_bkp_HAT_LL(node);
                    hasUpdate = true;
                    continue;
                }

                else
                {
                    //printf("\nEntry Discarded in Backup table due to higher path cost.\n");
                }
            }
            previous = current;
            current = current->next;
        }

        if (flag == 0) // That entry is not present. So added as first backup path for that mac address
        {
            add_entry_bkp_HAT_LL(node);
            hasUpdate = true;
        }
    }

    else // This is the first entry in the bkp table since the table is empty. Have to add the entry in bkp table
    {
        add_entry_bkp_HAT_LL(node);
        hasUpdate = true;
    }

    return hasUpdate;
}


/************************ MTP PDU CREATION FUNCTIONS ************************/
/*
 *   VID Advertisment - This message is sent when a JOIN message is received from non MT Switch
 *   @input
 *   param1 -   uint8_t       payload buffer
 *
 *   @output
 *   payloadLen
 *
 */
// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS>  <PATH COST> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
int build_VID_ADVT_PAYLOAD(uint8_t *data, char *interface, int treeNo)
{
    int payloadLen = 4;
    int numAdvts = 0;
    int egressPort = 0;
    int i = 0;

    if(treeNo == 1){
        current = primary_vid_tbl_head;
    }
    else{
        current = secondary_vid_tbl_head;
    }

    /*
     * grabs the ascii value for each char in the interface name (ex: e, t, h, 1), [48 ascii = 0 decimal, 57 ascii = 9 decimal]
     * if the ascii value is in the range of the ascii values for 0-9, move the decimal number to the egress port and assign it
     */
    for(; interface[i]!='\0'; i++)
    {
        //print the interface here to check if eth0
        //printf("interface being checked to build payload: %s\n", &interface[i]);
        if(interface[i] > 48 && interface[i] <= 57)
        {
            egressPort = (egressPort * 10) + (interface[i] - 48); //for example, (0 * 10) + (50 - 48) = 2 (the egress port)
            //printf("this is the egress port for VID: %d", egressPort);
        }
    }

    //while (current != NULL && current->membership <= 3)
    while (current != NULL)
    {
        //need to add statement to disregard if egress port is 0
        //9/20/17 - issue was fixed by just adding the continue statement and removing the current = current->next
        if(egressPort == 0)
        {
            continue;
        }

        char vid_addr[VID_ADDR_LEN];

        // <PATH COST> - Taken as '1' for now
        data[payloadLen] = (uint8_t) (current->path_cost + 1);

        // next byte
        payloadLen = payloadLen + 1;

        memset(vid_addr, '\0', VID_ADDR_LEN);

        // Sending VID out of the same port with the same VID so the upstream MTS knows it can add it to their CPVID table
        if (strncmp(interface, current->eth_name, strlen(interface)) == 0)
        {
            sprintf(vid_addr, "%s", current->vid_addr);
        }

        else
        {
            //snprintf(vid_addr, sizeof(vid_addr), "%s.%d", current->vid_addr, egressPort);
            sprintf(vid_addr, "%s.%d", current->vid_addr, egressPort);
        }

        // <VID_ADDR_LEN>
        data[payloadLen] = strlen(vid_addr);

        // next byte
        payloadLen = payloadLen + 1;

        memcpy(&data[payloadLen], vid_addr, strlen(vid_addr));

        payloadLen += strlen(vid_addr);
        current = current->next;

        numAdvts++;

        /* VID Advts should not be more than 3, because we need to advertise only the entries that are in Main VID Table
           from 3 we consider every path as backup path.
            */
        if (numAdvts == 3)
        {
            break;
        }
    }

    if (numAdvts > 0)
    {
        // <MSG_TYPE> - VID Advertisment, Type - 3.
        data[0] = (uint8_t) MTP_TYPE_VID_ADVT;

        // <OPERATION>
        data[1] = VID_ADD;

        // <NUMBER_VIDS> - Number of VID's
        data[2] = (uint8_t) numAdvts;

        data[3] = (uint8_t) treeNo;
    }

    else
    {
        payloadLen = 0;
    }

    return payloadLen;
}


/*
 *   build_VID_CHANGE_PAYLOAD - This message is sent when ever we miss periodic message for a time of 3 * hello_time.
 *                              Respective VID is deleted and adjacent peers who have VID derived from deleted VID are notified.
 *
 *   @input
 *   param1     -   uint8_t           payload buffer
 *   interface  -   char              interface name
 *   deletedVIDs-   char**            all recently deleted VIDs
 *   numberOfDel-   int               total number of deletions deletedVIDs reference pointing to.
 *
 *   @output
 *   payloadLen
 *	 Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
 */
int  build_VID_CHANGE_PAYLOAD(uint8_t *data, char *interface, char **deletedVIDs, int numberOfDeletions, int treeNo)
{
    int payloadLen = 4;
    int numAdvts = 0;
    int egressPort = 0;

    int i = 0;
    for(; interface[i]!='\0'; i++)
    {
        if(interface[i] >= 48 && interface[i] <= 57)
        {
            egressPort = (egressPort * 10) + (interface[i] - 48);
        }
    }

    i = 0;
    for (; i < numberOfDeletions; i++)
    {
        char vid_addr[VID_ADDR_LEN];

        memset(vid_addr, '\0', VID_ADDR_LEN);
        //sprintf(vid_addr, "%s.%d", deletedVIDs[i], egressPort ); for now, lets see if doesn't append egress port
        sprintf(vid_addr, "%s", deletedVIDs[i]);

        // <VID_ADDR_LEN>
        data[payloadLen] = strlen(vid_addr);

        // next byte
        payloadLen = payloadLen + 1;

        memcpy(&data[payloadLen], vid_addr, strlen(vid_addr));
        payloadLen += strlen(vid_addr);
        numAdvts++;
    }

    if (numAdvts > 0)
    {
        // <MSG_TYPE> - Advertisment Message, Type - 3.
        data[0] = (uint8_t) MTP_TYPE_VID_ADVT;

        // <OPERATION>
        data[1] = VID_DEL;

        // <NUMBER_VIDS> - Number of VID's
        data[2] = (uint8_t) numAdvts;

        data[3] = (uint8_t) treeNo;
    }

    else
    {
        payloadLen = 0;
    }

    return payloadLen;
}


/*
 *   build_JOIN_MSG_PAYLOAD - Join message sent when a non MTP switch wants to be
 *			      be part of MT_Topology. This switch Main VID Table is
 * 			      empty intially.
 *   @input
 *   param1 - 	uint8_t       payload buffer
 *
 *   @output
 *   payloadLen
 *
 */
// Message ordering <MSG_TYPE>
int build_JOIN_MSG_PAYLOAD(uint8_t *data)
{
    int payloadLen = 0;

    // <MSG_TYPE> - Hello Join Message, Type - 1.
    data[payloadLen] = (uint8_t) MTP_TYPE_JOIN_MSG;

    // next byte
    payloadLen = payloadLen + 1;

    // Return the total payload Length.
    return payloadLen;
}


/*
 *   build_PERIODIC_MSG_PAYLOAD - This message is sent every 2 seconds to inform connected
 *                                peers about its perscence.
 *
 *   @input
 *   param1 -   uint8_t           payload buffer
 *
 *   @output
 *   payloadLen
 *
 */
int  build_PERIODIC_MSG_PAYLOAD(uint8_t *data, int treeNo)
{
    int payloadLen = 0;

    // <MSG_TYPE> - Hello Periodic Message, Type - 2.
    data[payloadLen] = (uint8_t) MTP_TYPE_PERODIC_MSG;

    // next byte
    payloadLen = payloadLen + 1;

    data[payloadLen] = (uint8_t) treeNo;

    payloadLen = payloadLen + 1;

    // Return the total payload Length.
    return payloadLen;
}


int build_HAAdvt_message(uint8_t *data, struct ether_addr mac, uint8_t cost, uint8_t sequence_number, struct ether_addr *switch_id)
{
    int payloadLen = 1; // Index starts at the second byte of the header so the message type can come first

    //struct Host_Address_tuple *current = HAT_head;
    data[payloadLen] = (uint8_t) (cost);
    payloadLen = payloadLen + 1;

    data[payloadLen] = (uint8_t) (sequence_number);
    payloadLen = payloadLen + 1; // Is this pushing it off by one? There doesn't seem to be an issue, but there could be

    /* Host MAC Address */
    data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[0];
    data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[1];
    data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[2];
    data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[3];
    data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[4];
    data[payloadLen++] = (uint8_t ) mac.ether_addr_octet[5];

    /* Meshed Tree Switch ID */
    data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[0];
    data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[1];
    data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[2];
    data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[3];
    data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[4];
    data[payloadLen++] = (uint8_t ) switch_id->ether_addr_octet[5];

    data[0] = MTP_HAAdvt_TYPE; // Adding the Message Type field as the first Octet

    return payloadLen;
}


/************************ MTP PDU TRANSMISSION FUNCTIONS ************************/
void floodNotificationPDU(int msgType, int treeNo)
{
    int payloadLen = 0;
    uint8_t *payload = NULL;
    payload = (uint8_t*) calloc(1, MAX_BUFFER_SIZE);
    bool floodHostPorts = false, floodMTPPorts = false;

    if(msgType == MTP_TYPE_JOIN_MSG)
    {
        payloadLen = build_JOIN_MSG_PAYLOAD(payload);
        floodHostPorts = true;
        floodMTPPorts = true;
    }

    else if(msgType == MTP_TYPE_PERODIC_MSG)
    {
        payloadLen = build_PERIODIC_MSG_PAYLOAD(payload, treeNo);
        floodMTPPorts = true;
    }

    else
    {
        printf("unknown type");
    }

    if(payloadLen)
    {
        if(floodMTPPorts)
        {
            struct control_ports *currentMTPPort;
            for(currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
            {
                if(currentMTPPort->isUP == true)
                {
                    ctrlSend(currentMTPPort->eth_name, payload, payloadLen);

                    char* msg = "PDU Transmitted on MTP Port";
                    MTPlog(msg, currentMTPPort->eth_name, MSG_SENT, msgType);
                }
            }
        }

        if(floodHostPorts)
        {
            struct local_bcast_tuple *currentHostPort;
            for(currentHostPort = local_bcast_head; currentHostPort != NULL; currentHostPort = currentHostPort->next)
            {
                if(currentHostPort->isUP == true)
                {
                    ctrlSend(currentHostPort->eth_name, payload, payloadLen);

                    char* msg = "PDU Transmitted on Host Port";
                    MTPlog(msg, currentHostPort->eth_name, MSG_SENT, msgType);
                }
            }
        }
    }

    free(payload);

    return;
}


void sendVIDUpdatePDU_ADD(char* interface, int treeNo)
{
    int payloadLen = 0;
    uint8_t *payload = NULL;

    if((strcmp(interface, ALL_MTP_PORTS)) == 0) // If it needs to be flooded on all MTP ports
    {
        struct control_ports *currentMTPPort;
        for(currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
        {
            if(currentMTPPort->isUP == true)
            {
                payload = (uint8_t*) calloc(1, MAX_BUFFER_SIZE);
                payloadLen = build_VID_ADVT_PAYLOAD(payload, currentMTPPort->eth_name, treeNo);
                if(payloadLen)
                {
                    ctrlSend(currentMTPPort->eth_name, payload, payloadLen);

                    char* msg = "PDU [Add] Transmitted on MTP Port";
                    MTPlog(msg, currentMTPPort->eth_name, MSG_SENT, MTP_TYPE_VID_ADVT);
                }
                free(payload);
            }
        }
    }

    else if((strcmp(interface, ALL_HOST_PORTS)) == 0) // If it needs to be flooded on all host ports
    {
        struct local_bcast_tuple *currentHostPort;
        for (currentHostPort = local_bcast_head; currentHostPort != NULL; currentHostPort = currentHostPort->next)
        {
            if(currentHostPort->isUP == true)
            {
                payload = (uint8_t*) calloc(1, MAX_BUFFER_SIZE);
                payloadLen = build_VID_ADVT_PAYLOAD(payload, currentHostPort->eth_name, treeNo);
                if(payloadLen)
                {
                    ctrlSend(currentHostPort->eth_name, payload, payloadLen);

                    char* msg = "PDU [Add] Transmitted on Host Port";
                    MTPlog(msg, currentHostPort->eth_name, MSG_SENT, MTP_TYPE_VID_ADVT);
                }
                free(payload);
            }
        }
    }

    else
    {
        payload = (uint8_t*) calloc(1, MAX_BUFFER_SIZE);
        payloadLen = build_VID_ADVT_PAYLOAD(payload, interface, treeNo);
        if(payloadLen)
        {
            ctrlSend(interface, payload, payloadLen);

            char* msg = "PDU [Add] Transmitted";
            MTPlog(msg, interface, MSG_SENT, MTP_TYPE_VID_ADVT);
        }
        free(payload);
    }

    return;
}


void sendVIDUpdatePDU_DEL(char** deletedVIDs, int numberOfDeletions, int treeNo) // This needs to be integrated into the link failure code ASAP!
{
    int payloadLen = 0;
    uint8_t *payload = NULL;

    struct control_ports *currentMTPPort;
    for(currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
    {
        if(currentMTPPort->isUP == true)
        {
            payload = (uint8_t*) calloc(1, MAX_BUFFER_SIZE);
            payloadLen = build_VID_CHANGE_PAYLOAD(payload, currentMTPPort->eth_name, deletedVIDs, numberOfDeletions, treeNo);
            if(payloadLen)
            {
                ctrlSend(currentMTPPort->eth_name, payload, payloadLen);

                char* msg = "PDU [Del] Transmitted on MTP Port";
                MTPlog(msg, currentMTPPort->eth_name, MSG_SENT, MTP_TYPE_VID_DEL_ADVT);
            }
            free(payload);
        }
    }

    return;
}


void sendHostUpdatePDU(struct Host_Address_tuple* HAT, char* recvOnEtherPort)
{
    int payloadLen = 0;
    uint8_t *payload = NULL;

    payload = (uint8_t*) calloc(1, MAX_BUFFER_SIZE);
    payloadLen = build_HAAdvt_message(payload, HAT->mac, HAT->path_cost, HAT->sequence_number, HAT->switch_id);

    struct control_ports *currentMTPPort;
    for(currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
    {
        if((strcmp(currentMTPPort->eth_name, recvOnEtherPort) != 0) && currentMTPPort->isUP == true)
        {
            ctrlSend(currentMTPPort->eth_name, payload, payloadLen);

            char* msg = "PDU [HAADVT] Transmitted on MTP Port";
            MTPlog(msg, currentMTPPort->eth_name, MSG_SENT, MTP_HAAdvt_TYPE);
        }
    }
    free(payload);

    return;
}


// Check if the mac is already in HAT table, if it is, switch the frame, if not, alert the calling function
bool attemptUnicastSwitching(uint8_t *rx_buffer, struct ether_header *head, int recv_len)
{
    struct Host_Address_tuple *current;
    for(current = HAT_head; current != NULL; current = current->next)
    {
        if(memcmp(&head->ether_dhost, &current->mac, sizeof(struct ether_addr)) == 0) // If MAC Address match found
        {
            //rx_buffer[15]++; - Increment Hop Count. WHY??? WHAT IS THIS DOING FOR AN END NODE?
            dataSend(current->eth_name, rx_buffer, recv_len);

            char switchStuff[18 + strlen(ether_ntoa(&current->mac))];
            snprintf(switchStuff, sizeof(switchStuff), "%s unicast switched", ether_ntoa(&current->mac));
            MTPlog(switchStuff, current->eth_name, MSG_INFO, MTP_HAAdvt_TYPE);

            return TRUE;
        }
    }

    return FALSE;
}


/************************ ENTRY FAILURE DETECTION FUNCTIONS ************************/
/**
 *              Check for link Failures.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              void
**/
int checkForPrimaryFailuresVID(char **deletedVIDs)
{
    struct vid_addr_tuple *current = primary_vid_tbl_head;
    struct vid_addr_tuple *previous = NULL;
    time_t currentTime = time(0);
    int numberOfFailures = 0;

    while(current != NULL)
    {
        if((current->last_updated != -1) && (currentTime - current->last_updated) > (2 * PERIODIC_HELLO_TIME))
        {
            /* Logging failure information */
            char VIDFailureInfo[34 + strlen(current->vid_addr)];
            if((currentTime - current->last_updated) >= 500)
            {
                snprintf(VIDFailureInfo, sizeof(VIDFailureInfo), "%s {%d} Failed From VID Table [int]", current->vid_addr, current->membership);
            }

            else
            {
                snprintf(VIDFailureInfo, sizeof(VIDFailureInfo), "%s {%d} Failed From VID Table [1MH]", current->vid_addr, current->membership);
            }
            MTPlog(VIDFailureInfo, current->eth_name, MSG_INFO, VID_REMOVED);
            /* Logging failure information */

            struct vid_addr_tuple *temp = current;
            deletedVIDs[numberOfFailures] = (char*)calloc(strlen(temp->vid_addr), sizeof(char));

            if (previous == NULL)
            {
                primary_vid_tbl_head	= current->next;
            }

            else
            {
                previous->next = current->next;
            }

            strncpy(deletedVIDs[numberOfFailures], temp->vid_addr, strlen(temp->vid_addr));
            current = current->next;
            numberOfFailures++;
            free(temp);
            continue;
        }
        previous = current;
        current = current->next;
    }

    // if failures are there
    if (numberOfFailures > 0)
    {
        int membership = 1;

        for (current = primary_vid_tbl_head; current != NULL; current = current->next)
        {
            current->membership = membership;
            membership++;
        }
    }

    return numberOfFailures;
}

int checkForSecondaryFailuresVID(char **deletedVIDs)
{
    struct vid_addr_tuple *current = secondary_vid_tbl_head;
    struct vid_addr_tuple *previous = NULL;
    time_t currentTime = time(0);
    int numberOfFailures = 0;

    while(current != NULL)
    {
        if((current->last_updated != -1) && (currentTime - current->last_updated) > (2 * PERIODIC_HELLO_TIME))
        {
            /* Logging failure information */
            char VIDFailureInfo[34 + strlen(current->vid_addr)];
            if((currentTime - current->last_updated) >= 500)
            {
                snprintf(VIDFailureInfo, sizeof(VIDFailureInfo), "%s {%d} Failed From VID Table [int]", current->vid_addr, current->membership);
            }

            else
            {
                snprintf(VIDFailureInfo, sizeof(VIDFailureInfo), "%s {%d} Failed From VID Table [1MH]", current->vid_addr, current->membership);
            }
            MTPlog(VIDFailureInfo, current->eth_name, MSG_INFO, VID_REMOVED);
            /* Logging failure information */

            struct vid_addr_tuple *temp = current;
            deletedVIDs[numberOfFailures] = (char*)calloc(strlen(temp->vid_addr), sizeof(char));

            if (previous == NULL)
            {
                secondary_vid_tbl_head	= current->next;
            }

            else
            {
                previous->next = current->next;
            }

            strncpy(deletedVIDs[numberOfFailures], temp->vid_addr, strlen(temp->vid_addr));
            current = current->next;
            numberOfFailures++;
            free(temp);
            continue;
        }
        previous = current;
        current = current->next;
    }

    // if failures are there
    if (numberOfFailures > 0)
    {
        int membership = 1;

        for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
        {
            current->membership = membership;
            membership++;
        }
    }

    return numberOfFailures;
}

/**
 *              Check for link Failures.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *primary_cpvid_tbl_head
 *
 *              @return
 *              void
**/
bool checkForPrimaryFailuresCPVID()
{
    struct child_pvid_tuple *current = primary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    time_t currentTime = time(0);
    bool hasDeletions = false;

    while(current != NULL)
    {
        // Hello Change org - if((current->last_updated !=-1) && (currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME))
        if((current->last_updated !=-1) && (currentTime - current->last_updated) > (2 * PERIODIC_HELLO_TIME))
        {
            /* Logging failure information */
            char CPVIDFailureInfo[31 + strlen(current->vid_addr)];
            if((currentTime - current->last_updated) >= 500)
            {
                snprintf(CPVIDFailureInfo, sizeof(CPVIDFailureInfo), "%s Failed From CPVID Table [int]", current->vid_addr);
            }

            else
            {
                snprintf(CPVIDFailureInfo, sizeof(CPVIDFailureInfo), "%s Failed From CPVID Table [1MH]", current->vid_addr);
            }
            MTPlog(CPVIDFailureInfo, current->child_port, MSG_INFO, VID_REMOVED);
            /* Logging failure information */

            struct child_pvid_tuple *temp = current;

            if(previous == NULL)
            {
                primary_cpvid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(temp);
            hasDeletions = true;

            continue;
        }
        previous = current;
        current = current->next;
    }

    return hasDeletions;
}

bool checkForSecondaryFailuresCPVID()
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    time_t currentTime = time(0);
    bool hasDeletions = false;

    while(current != NULL)
    {
        // Hello Change org - if((current->last_updated !=-1) && (currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME))
        if((current->last_updated !=-1) && (currentTime - current->last_updated) > (2 * PERIODIC_HELLO_TIME))
        {
            /* Logging failure information */
            char CPVIDFailureInfo[31 + strlen(current->vid_addr)];
            if((currentTime - current->last_updated) >= 500)
            {
                snprintf(CPVIDFailureInfo, sizeof(CPVIDFailureInfo), "%s Failed From CPVID Table [int]", current->vid_addr);
            }

            else
            {
                snprintf(CPVIDFailureInfo, sizeof(CPVIDFailureInfo), "%s Failed From CPVID Table [1MH]", current->vid_addr);
            }
            MTPlog(CPVIDFailureInfo, current->child_port, MSG_INFO, VID_REMOVED);
            /* Logging failure information */

            struct child_pvid_tuple *temp = current;

            if(previous == NULL)
            {
                secondary_cpvid_tbl_head = current->next;
            }

            else
            {
                previous->next = current->next;
            }

            current = current->next;
            free(temp);
            hasDeletions = true;

            continue;
        }
        previous = current;
        current = current->next;
    }

    return hasDeletions;
}

bool checkForFailuresHAT()
{
    struct Host_Address_tuple *current;
    struct Host_Address_tuple *bkpcurrent;

    struct Host_Address_tuple *temp_HAT = (struct Host_Address_tuple *) calloc(1, sizeof (struct Host_Address_tuple));
    struct Host_Address_tuple *temp_bkp_HAT = (struct Host_Address_tuple *) calloc(1, sizeof (struct Host_Address_tuple));

    time_t currentTime = time(0);
    int flag;
    char failed_inf[5];
    uint8_t *payload = NULL;
    bool hasUpdate = false;

    /* Search the main HAT for any failures */
    //for(; current != NULL; current = current->next)
    for(current = HAT_head; current != NULL; current = current->next)
    {
        flag = 0;

        if((current->time_current != -1) && (current->local == FALSE) && ((currentTime - current->time_current) > (2 * PERIODIC_HELLO_TIME)))
        {
            /* Logging failure information */
            char HATFailureInfo[35 + strlen(ether_ntoa(&current->mac))];
            if((currentTime - current->time_current) >= 500)
            {
                snprintf(HATFailureInfo, sizeof(HATFailureInfo), "%s Failed From Main Host Table [int]", ether_ntoa(&current->mac));
            }

            else
            {
                snprintf(HATFailureInfo, sizeof(HATFailureInfo), "%s Failed From Main Host Table [1MH]", ether_ntoa(&current->mac));
            }
            MTPlog(HATFailureInfo, current->eth_name, MSG_INFO, HAT_REMOVED);

            /* Copy the failed entry and its interface into temporary storage spaces */
            memcpy(temp_HAT, current, sizeof(struct Host_Address_tuple));
            strncpy(failed_inf, current->eth_name, strlen(current->eth_name));

            /* Search the backup HAT for any failures if a main HAT failure is found */
            //for(; bkpcurrent != NULL; bkpcurrent = bkpcurrent->next)
            for(bkpcurrent = HAT_bkp_head; bkpcurrent != NULL; bkpcurrent = bkpcurrent->next)
            {
                if(memcmp(&current->mac, &bkpcurrent->mac, sizeof(struct ether_addr)) == 0)
                {
                    flag = 1; // Change flag to signal that a backup entry was found in the HAT

                    memcpy(temp_bkp_HAT, bkpcurrent, sizeof(struct Host_Address_tuple)); //copy that backup entry into temporary structure
                    temp_bkp_HAT->sequence_number++; // Increment seq number before sending.

                    delete_entry_HAT_LL(current); // Delete the failed entry from HAT.

                    struct Host_Address_tuple *node = (struct Host_Address_tuple *) calloc(1, sizeof (struct Host_Address_tuple));
                    memcpy(node, bkpcurrent, sizeof(struct Host_Address_tuple)); // Copy the backup which needs to be promoted to the main HAT

                    /* Promote the backup entry to the main entry, and delete the entry from the backup table */
                    add_entry_HAT_LL(node);
                    delete_entry_bkp_HAT_LL(bkpcurrent);

                    hasUpdate = true;
                    break;
                    //bkpcurrent=temp_bkp_HAT;
                }
            }

            if(flag == 0) // If the backup is not found we only have to delete the main entry
            {
                //memcpy(temp_HAT,current,sizeof(struct Host_Address_tuple));
                temp_HAT->sequence_number++; // Increment sequence number before deleting.
                temp_HAT->path_cost = 100;   // Cost 100 will delete the entries in other switches since it violates 99 rule.

                delete_entry_HAT_LL(current); // Delete from main entry itself.
                hasUpdate = true;
            }

            if(flag)
            {
                sendHostUpdatePDU(temp_bkp_HAT, failed_inf);
            }

            else
            {
                sendHostUpdatePDU(temp_HAT, failed_inf);
            }
            current = temp_HAT; //This provides the link for current->next since the LL we are using in for loop is altered.
        }
    }

    /*bkpcurrent = getInstance_HAT_bkp_LL();
    //printf("\nGot Backup Head.\n");
    for (; bkpcurrent != NULL; bkpcurrent = bkpcurrent->next) {
      if ((bkpcurrent->time_current != -1) &&(currentTime - bkpcurrent->time_current) > (3 * PERIODIC_HELLO_TIME)) {
          memcpy(temp_bkp_HAT,bkpcurrent,sizeof(struct Host_Address_tuple));
          printf("\nBackup Entry Expired with %s.\n",bkpcurrent->eth_name);
          delete_entry_bkp_HAT_LL(bkpcurrent);
          printf("\n9\n");
          bkpcurrent=temp_bkp_HAT;
          printf("\n10\n");
          printBackupHostAddressTable();

      }
    }*/

    free(temp_HAT);
    free(temp_bkp_HAT);

    return hasUpdate;
}


/************************ INTERFACE STATUS/FAILURE DETECTION FUNCTIONS ************************/
bool checkInterfaceIsActive(char *str)
{
    struct ifaddrs *ifaddr, *ifa; // find all interfaces on the node

    if(getifaddrs(&ifaddr))
    {
        perror("Error: getifaddrs Failed\n");
        exit(0);
    }

    // loop through the list
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        int family;
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET && (strncmp(ifa->ifa_name, str, strlen(str)) == 0) && (ifa->ifa_flags & IFF_UP) != 0)
        {
            freeifaddrs(ifaddr);
            return true;
        }
    }
    freeifaddrs(ifaddr);

    return false;
}


void checkForPrimaryLinkFailure()
{
    struct control_ports *currentMTPPort;
    for(currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
    {
        if(currentMTPPort->isUP == true && checkInterfaceIsActive(currentMTPPort->eth_name) != true) // If the link has gone down
        {
            currentMTPPort->isUP = false;

            char LinkFailureInfo[18];
            snprintf(LinkFailureInfo, sizeof(LinkFailureInfo), "Interface Failure");
            MTPlog(LinkFailureInfo, currentMTPPort->eth_name, MSG_INFO, FAILURE_ALERT);

            /* Check the Main and Backup VID Tables */
            struct vid_addr_tuple *currentVID;
            for(currentVID = primary_vid_tbl_head; currentVID != NULL; currentVID = currentVID->next)
            {
                if(strcmp(currentVID->eth_name, currentMTPPort->eth_name) == 0) // If the interface names match
                {
                    currentVID->last_updated -= 100000;
                }
            }

            /* Check the CPVID Table */
            struct child_pvid_tuple *currentCPVID;
            for(currentCPVID = primary_cpvid_tbl_head; currentCPVID != NULL; currentCPVID = currentCPVID->next)
            {
                if(strcmp(currentCPVID->child_port, currentMTPPort->eth_name) == 0) // If the interface names match
                {
                    currentCPVID->last_updated -= 100000;
                }
            }

            /* Check the Host Address Table */
            struct Host_Address_tuple *currentHostEntry;
            for(currentHostEntry = HAT_head; currentHostEntry != NULL; currentHostEntry = currentHostEntry->next)
            {
                if(strcmp(currentHostEntry->eth_name, currentMTPPort->eth_name) == 0) // If the interface names match
                {
                    currentHostEntry->time_current -= 100000;
                }
            }
        }
    }

    return;
}

void checkForSecondaryLinkFailure()
{
    struct control_ports *currentMTPPort;
    for(currentMTPPort = control_ports_head; currentMTPPort != NULL; currentMTPPort = currentMTPPort->next)
    {
        if(currentMTPPort->isUP == true && checkInterfaceIsActive(currentMTPPort->eth_name) != true) // If the link has gone down
        {
            currentMTPPort->isUP = false;

            char LinkFailureInfo[18];
            snprintf(LinkFailureInfo, sizeof(LinkFailureInfo), "Interface Failure");
            MTPlog(LinkFailureInfo, currentMTPPort->eth_name, MSG_INFO, FAILURE_ALERT);

            /* Check the Main and Backup VID Tables */
            struct vid_addr_tuple *currentVID;
            for(currentVID = secondary_vid_tbl_head; currentVID != NULL; currentVID = currentVID->next)
            {
                if(strcmp(currentVID->eth_name, currentMTPPort->eth_name) == 0) // If the interface names match
                {
                    currentVID->last_updated -= 100000;
                }
            }

            /* Check the CPVID Table */
            struct child_pvid_tuple *currentCPVID;
            for(currentCPVID = secondary_cpvid_tbl_head; currentCPVID != NULL; currentCPVID = currentCPVID->next)
            {
                if(strcmp(currentCPVID->child_port, currentMTPPort->eth_name) == 0) // If the interface names match
                {
                    currentCPVID->last_updated -= 100000;
                }
            }

            /* Check the Host Address Table */
            struct Host_Address_tuple *currentHostEntry;
            for(currentHostEntry = HAT_head; currentHostEntry != NULL; currentHostEntry = currentHostEntry->next)
            {
                if(strcmp(currentHostEntry->eth_name, currentMTPPort->eth_name) == 0) // If the interface names match
                {
                    currentHostEntry->time_current -= 100000;
                }
            }
        }
    }

    return;
}

/************************ HOST ADDRESS TABLE SUPPORT FUNCTIONS ************************/
struct ether_addr* get_switchid()
{
    struct control_ports *current;
    int fd, i = 0, j, k, flag = 0;
    struct ifreq ifr;
    char *iface;
    struct ether_addr *temp_mac =(struct ether_addr *) calloc (1, sizeof (struct ether_addr));

    for(current = control_ports_head; (current != NULL || flag != 1); current = current->next)
    {
        if(current == NULL && flag == 0) // This maybe is supposed to be just the flag part? Not NULL?
        {
            current = (struct control_ports *) local_bcast_head; // Is this switching control to the other table? I don't think it makes sense
            flag = 1;

            if(current == NULL) // Redundant and doesn't make sense
            {
                break;
            }
        }

        struct ether_addr *mac = (struct ether_addr *) calloc(1, sizeof (struct ether_addr));
        iface = current->eth_name;
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        ifr.ifr_addr.sa_family = AF_INET;

        strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);
        ioctl(fd, SIOCGIFHWADDR, &ifr);
        close(fd);

        mac = (struct ether_addr *)ifr.ifr_hwaddr.sa_data;

        if(i == 0)
        {
            for(k = 0; k < 6; k++)
            {
                temp_mac->ether_addr_octet[k] = mac->ether_addr_octet[k];
            }

            i = 1;
        }

        else
        {
            for(j = 0; j != 6; j++)
            {
                if((mac->ether_addr_octet[j])>(temp_mac->ether_addr_octet[j]))
                {
                    break;
                }

                else if((mac->ether_addr_octet[j])<(temp_mac->ether_addr_octet[j]))
                {
                    for(k = 0; k < 6; k++)
                    {
                        temp_mac->ether_addr_octet[k]=mac->ether_addr_octet[k];
                    }
                    break;
                }
            } // End of inner for loop
        } // End of else
    } // End of outter For loop

    return temp_mac;
}


// This function compares the sequence number.
bool check_cost99_sequence_no(struct Host_Address_tuple *node)
{
    struct Host_Address_tuple *current;
    struct Host_Address_tuple *bkpcurrent;
    int flag = 0;

    for(current = HAT_head; current != NULL; current = current->next)
    {
        if(memcmp(&node->mac, &current->mac, sizeof(struct ether_addr)) == 0)
        {
            if(node->sequence_number > current->sequence_number) // Checks for higher sequence number for recent entry.
            {
                if(node->path_cost < 99) // Check for path cost 99
                {
                    if(node->path_cost <= current->path_cost)
                    {
                        delete_entry_HAT_LL(current);
                        add_entry_HAT_LL(node);
                        //printf("\nHAT entry with same details but greater sequence number and same of less path cost. So update in main table.\n");
                        return true;
                    }

                    else // Now checking in backup table with path cost for updating
                    {
                        for(bkpcurrent = HAT_bkp_head; bkpcurrent != NULL; bkpcurrent = bkpcurrent->next)
                        {
                            flag = 2;

                            if(memcmp(&node->mac, &bkpcurrent->mac, sizeof(struct ether_addr)) == 0)
                            {
                                flag = 1;

                                if(node->path_cost > bkpcurrent->path_cost)
                                {
                                    delete_entry_HAT_LL(current);
                                    struct Host_Address_tuple *temp = (struct Host_Address_tuple *) calloc (1, sizeof (struct Host_Address_tuple));
                                    memcpy(temp,bkpcurrent,sizeof(struct Host_Address_tuple));
                                    add_entry_HAT_LL(temp);

                                    delete_entry_bkp_HAT_LL(bkpcurrent);
                                    add_entry_bkp_HAT_LL(node);
                                    //printf("\nNew is higher than lowest backup entry. Hence the entries are swapped\n");
                                    return true;
                                }

                                else
                                {
                                    delete_entry_HAT_LL(current);
                                    add_entry_HAT_LL(node);
                                    //printf("\nNew entry is lower or same than the lowest entry in backup. Hence main entry is updated,\n");
                                    return true;
                                }
                            }
                        }

                        if(flag != 1) // If no backup entry, then add in main table
                        {
                            delete_entry_HAT_LL(current);
                            add_entry_HAT_LL(node);
                            return true;
                        }
                        //printf("\nNew entry with same details checked in both main and backup table and updated accordingly.\n");
                    }
                }

                else // If cost 99 reached
                {
                    flag = 0;
                    //printf("\nCost reached value 99. So removing entry from the list. And updating from backup.\n");

                    char HATNineNineInfo[24 + strlen(ether_ntoa(&node->mac))];
                    snprintf(HATNineNineInfo, sizeof(HATNineNineInfo), "%s Exceeded Max Cost [%d]", ether_ntoa(&node->mac), node->path_cost);
                    MTPlog(HATNineNineInfo, node->eth_name, MSG_INFO, HAT_REMOVED);

                    for(bkpcurrent = HAT_bkp_head; bkpcurrent != NULL; bkpcurrent = bkpcurrent->next)
                    {
                        if(memcmp(&node->mac, &bkpcurrent->mac, sizeof(struct ether_addr)) == 0)
                        {
                            flag = 1;
                            delete_entry_HAT_LL(current);
                            struct Host_Address_tuple *temp = (struct Host_Address_tuple *) calloc (1, sizeof (struct Host_Address_tuple));
                            memcpy(temp,bkpcurrent,sizeof(struct Host_Address_tuple));
                            add_entry_HAT_LL(temp);
                            delete_entry_bkp_HAT_LL(bkpcurrent); //and delete it.
                            //printf("\nBackup entry transfered to main HAT entry.\n");
                            return true;
                        }
                    }

                    if(flag == 0)
                    {
                        delete_entry_HAT_LL(node); //if no entry in backup also, delete from main entry itself.
                        //printf("\nNo entry found in Backup table also. So the entry is completely removed from main table.\n");
                        return true;
                    }
                } // 99 path cost else
            } // sequence check if
        } // HAT mac match in main entry
    } // main table for loop

    return false;
}


/************************ TABLE PRINTING FUNCTIONS ************************/
/**
 *		Print VID Table.
 *      	VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		void
**/
void printPrimaryVIDTable()
{
    struct vid_addr_tuple *current;
    int tracker = MAX_MAIN_VID_TBL_PATHS;
    int lenCurrentVID;

    printf("%s\n", "####### Main VID Table #########");
    printf("%s %30s %10s %10s %5s\n","| MT_VID |", "| Interface Name |", "| Path Cost |", "| Membership |", "| Source MAC Address |");

    for (current = primary_vid_tbl_head; current != NULL && current->membership <= MAX_MAIN_VID_TBL_PATHS; current = current->next)
    {
        lenCurrentVID = strlen(current->vid_addr);
        printf("%*s %*s %15d %13d %29s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac));
    }
}

void printSecondaryVIDTable()
{
    struct vid_addr_tuple *current;
    int tracker = MAX_MAIN_VID_TBL_PATHS;
    int lenCurrentVID;

    printf("%s\n", "####### Main VID Table #########");
    printf("%s %30s %10s %10s %5s\n","| MT_VID |", "| Interface Name |", "| Path Cost |", "| Membership |", "| Source MAC Address |");

    for (current = secondary_vid_tbl_head; current != NULL && current->membership <= MAX_MAIN_VID_TBL_PATHS; current = current->next)
    {
        lenCurrentVID = strlen(current->vid_addr);
        printf("%*s %*s %15d %13d %29s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac));
    }
}

/**
 *    Print VID Table.
 *    Backup VID Paths,    Implemented Using linked list, instead of maintaining a seperate table, I am adding Main VIDS and Backup Paths
 *                         in the same table.
 *    Head Ptr,   *vid_table
 *
 *    @return
 *    void
**/
void printPrimaryBackupVIDTable()
{
    struct vid_addr_tuple *current;
    int lenCurrentVID;

    printf("\n%s\n", "####### Backup VID Table #########");
    printf("%s %30s %10s %10s %5s\n","| MT_VID |", "| Interface Name |", "| Path Cost |", "| Membership |", "| Source MAC Address |");

    for (current = primary_vid_tbl_head; current != NULL; current = current->next)
    {
        if(current->membership > MAX_MAIN_VID_TBL_PATHS)
        {
            lenCurrentVID = strlen(current->vid_addr);
            printf("%*s %*s %15d %13d %29s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac));
        }
    }
}

void printSecondaryBackupVIDTable()
{
    struct vid_addr_tuple *current;
    int lenCurrentVID;

    printf("\n%s\n", "####### Backup VID Table #########");
    printf("%s %30s %10s %10s %5s\n","| MT_VID |", "| Interface Name |", "| Path Cost |", "| Membership |", "| Source MAC Address |");

    for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
    {
        if(current->membership > MAX_MAIN_VID_TBL_PATHS)
        {
            lenCurrentVID = strlen(current->vid_addr);
            printf("%*s %*s %15d %13d %29s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac));
        }
    }
}


/**
 *    Print Child PVID  Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *
 *    @return
 *    void
**/
void printPrimaryChildPVIDTable()
{
    struct child_pvid_tuple *current;
    int lenCurrentVID;

    printf("\n%s\n", "####### Child PVID Table #########");
    printf("%s %26s %10s\n","| Child PVID |", "| Interface Name |", "| Source MAC Address |");

    for(current = primary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        lenCurrentVID = strlen(current->vid_addr);
        printf("%*s %*s %30s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->child_port, ether_ntoa(&current->mac));
    }
}

void printSecondaryChildPVIDTable()
{
    struct child_pvid_tuple *current;
    int lenCurrentVID;

    printf("\n%s\n", "####### Child PVID Table #########");
    printf("%s %26s %10s\n","| Child PVID |", "| Interface Name |", "| Source MAC Address |");

    for(current = secondary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        lenCurrentVID = strlen(current->vid_addr);
        printf("%*s %*s %30s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->child_port, ether_ntoa(&current->mac));
    }
}
/**
 *    Print local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    void
**/
void printLocalHostBroadcastTable()
{
    struct local_bcast_tuple *current;

    printf("\n%s\n", "####### Host Interfaces #########");
    printf("%s %10s\n", "| Interface Name |", "| status |");

    for (current = local_bcast_head; current != NULL; current = current->next)
    {
        printf("%6s %15d\n", current->eth_name, current->isUP);
    }
}


/**
 *    Print Control Ports Table.
 *    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    void
**/
void printControlPortsTable()
{
    struct control_ports *current;

    printf("\n####### Control Interfaces #########\n");
    printf("%s %10s\n", "| Interface Name |", "| status |");

    for (current = control_ports_head; current != NULL; current = current->next)
    {
        printf("%6s %15d\n", current->eth_name, current->isUP);
    }
}


void printPathCostTable()
{
    struct path_cost_tuple *current;

    printf("\n####### Path Cost Table #########\n");
    printf("%s %8s\n", "| Interface Name |", "| cost |");

    for (current = path_cost_head; current != NULL; current = current->next)
    {
        printf("%6s %15d\n", current->eth_name, current->path_cost);
    }
}


void printMainHostAddressTable()
{
    /*
      The functions ether_ntoa_r() and ether_aton_r() are reentrant thread-safe versions of ether_ntoa() and ether_aton() respectively,
      and do not use static buffers. Static buffers being if you call ether_ntoa twice in one printf, it overwrites the second one with
      The first. This is why it was printing the Host MAC address for both the Host MAC field and the Switch ID field.

      static char buf[18];
      return ether_ntoa_r(addr, buf);
    */
    struct Host_Address_tuple *current;

    printf("\n####### Host Address Table #########\n");
    printf("%s %12s %25s %8s %8s %8s\n", "| Host MAC Address |", "| Switch ID |", "| Interface Name |", "| Cost |", "| Seq Num |", "| Local |");

    for(current = HAT_head; current != NULL; current = current->next)
    {
        static char MACBuf[18];
        static char IDBuf[18];
        printf("%5s %20s %10s %15d %8d %11d\n", ether_ntoa_r(&current->mac, MACBuf), ether_ntoa_r((struct ether_addr*)&current->switch_id->ether_addr_octet, IDBuf), current->eth_name, current->path_cost, current->sequence_number, current->local);
    }

    /*
    for (current = HAT_head; current != NULL; current = current->next)
      {
      printf("port name \t\t cost \t\t seq num \t\t mac\t\t local \t\t switch-id\n");
          printf("%s\t\t\t%d\t\t%d\t\t%s\t\t%d\t\t", current->eth_name, current->path_cost,  current->sequence_number, ether_ntoa(&current->mac), current->local);
          printf("%s\n",ether_ntoa((struct ether_addr*)&current->switch_id->ether_addr_octet));
      }
    */
}


void printBackupHostAddressTable()
{
    struct Host_Address_tuple *current;

    printf("\n####### Backup Host Address Table #########\n");
    printf("%s %12s %25s %8s %8s %8s\n", "| Host MAC Address |", "| Switch ID |", "| Interface Name |", "| Cost |", "| Seq Num |", "| Local |");

    for(current = HAT_bkp_head; current != NULL; current = current->next)
    {
        static char MACBuf[18];
        static char IDBuf[18];
        printf("%5s %20s %10s %15d %8d %11d\n", ether_ntoa_r(&current->mac, MACBuf), ether_ntoa_r((struct ether_addr*)&current->switch_id->ether_addr_octet, IDBuf), current->eth_name, current->path_cost, current->sequence_number, current->local);
    }

    /*
      for (current = HAT_bkp_head; current != NULL; current = current->next)
      {
      printf("port name \t\t cost \t\t seq num \t\t mac\t\t local \t\t switch-id\n");
          printf("%s\t\t\t%d\t\t%d\t\t%s\t\t%d\t\t", current->eth_name, current->path_cost,  current->sequence_number, ether_ntoa(&current->mac), current->local);
          printf("%s\n",ether_ntoa((struct ether_addr*)&current->switch_id->ether_addr_octet));
      }
    */
}


void printTablesPrimary()
{
    printf("------------------------------------------------------------------------\n");
    printPrimaryVIDTable();         		// MAIN VID TABLE
    printPrimaryBackupVIDTable();     			// BACKUP VID TABLE
    printPrimaryChildPVIDTable();   				// CHILD PVID TABLE
    printLocalHostBroadcastTable(); // LOCAL HOST PORTS TABLE
    printControlPortsTable();				// CONTROL PORTS TABLE
    printPathCostTable();						// PATH COST TABLE
    printMainHostAddressTable();    // MAIN HOST ADDRESS TABLE
    printBackupHostAddressTable();  // BACKUP HOST ADDRESS TABLE
    printf("------------------------------------------------------------------------\n");

    return;
}

void printTablesSecondary()
{
    printf("------------------------------------------------------------------------\n");
    printSecondaryVIDTable();         		// MAIN VID TABLE
    printSecondaryBackupVIDTable();     			// BACKUP VID TABLE
    printSecondaryChildPVIDTable();   				// CHILD PVID TABLE
    printLocalHostBroadcastTable(); // LOCAL HOST PORTS TABLE
    printControlPortsTable();				// CONTROL PORTS TABLE
    printPathCostTable();						// PATH COST TABLE
    printMainHostAddressTable();    // MAIN HOST ADDRESS TABLE
    printBackupHostAddressTable();  // BACKUP HOST ADDRESS TABLE
    printf("------------------------------------------------------------------------\n");

    return;
}


void print_HAAdvt_message_content(uint8_t *rx_buffer)
{
    uint8_t mac_addr[6], temp[6];

    mac_addr[0] = rx_buffer[17];
    mac_addr[1] = rx_buffer[18];
    mac_addr[2] = rx_buffer[19];
    mac_addr[3] = rx_buffer[20];
    mac_addr[4] = rx_buffer[21];
    mac_addr[5] = rx_buffer[22];
    temp[0] = rx_buffer[23];
    temp[1] = rx_buffer[24];
    temp[2] = rx_buffer[25];
    temp[3] = rx_buffer[26];
    temp[4] = rx_buffer[27];
    temp[5] = rx_buffer[28];

    printf("\nRECEIVED HAA MESSAGE:\nCost is %d, Sequence number is %d, MAC address is %s \n", (uint8_t) rx_buffer[15], (uint8_t) rx_buffer[16], ether_ntoa((struct ether_addr *) mac_addr));
    printf("Switch Id is %s\n", ether_ntoa((struct ether_addr *) temp));
}


/************************ IN PROGRESS PROTOTYPE FUNCTIONS ************************/
void MTPlog(char* logMessage, char* interfaceName, int direction, int messageType)
{
    clockid_t clk_id = CLOCK_REALTIME;
    const uint TIME_FORMAT_LENGTH = strlen("2000-12-31 12:59:59.123456789") + 1;
    char timeStampFormat[TIME_FORMAT_LENGTH];
    struct timespec ts;
    clock_gettime(clk_id, &ts);

    if(getTimeStamp(timeStampFormat, sizeof(timeStampFormat), &ts) != 0)
    {
        printf("getTimeStamp failed!\n");
    }

    else
    {
        FILE *logFile = fopen("mtpd.log", "a");
        fprintf(logFile, "%s|%s|%d|%d|%s\n", timeStampFormat, interfaceName, direction, messageType, logMessage);
        //fflush(logFile);
        fclose(logFile);
    }

    return;
}


int getTimeStamp(char *buf, uint len, struct timespec *ts)
{
    int ret;
    struct tm t;

    tzset();

    if(localtime_r(&(ts->tv_sec), &t) == NULL)
    {
        return 1;
    }

    ret = strftime(buf, len, "%F %T", &t);
    if(ret == 0)
    {
        return 2;
    }

    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if(ret >= len)
    {
        return 3;
    }

    return 0;
}


bool isIdenticalMACAddress(struct ether_addr *mac1, struct ether_addr *mac2)
{
    bool sameAddress = false;

    if(memcmp(mac1, mac2, sizeof(struct ether_addr)) == 0)
    {
        sameAddress = true;
    }

    return sameAddress;
}
