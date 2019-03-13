#include "feature_payload.h"
#include "mtp_send.h"

extern struct timespec start, end;
struct vid_addr_tuple *primary_vid_tbl_head = NULL;
struct child_pvid_tuple *primary_cpvid_tbl_head = NULL;

struct vid_addr_tuple *secondary_vid_tbl_head = NULL;
struct child_pvid_tuple *secondary_cpvid_tbl_head = NULL;
struct local_bcast_tuple *local_bcast_head = NULL;

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
	//checking only the Primary VID table.
	if(primary_vid_tbl_head != NULL)
	{
		struct vid_addr_tuple *current = primary_vid_tbl_head;
		int lenInputVID = strlen(vid);

		while(current != NULL)
		{
    	int lenCurrentVID = strlen(current->vid_addr);

			// This check is mainly if we get a parent ID, we have eliminate this as the VID is a parent ID.
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

            // This check is mainly if we get a parent ID, we have eliminate this as the VID is a parent ID.
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
 *   VID Advertisment - This message is sent when a JOIN message is received from non MT Switch
 *   @input
 *   param1 -   uint8_t       payload buffer
 *
 *   @output
 *   payloadLen
 *
 */
// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS>  <PATH COST> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
int  build_VID_ADVT_PAYLOAD(uint8_t *data, char *interface, int treeNo)
{
  int payloadLen = 4;
  int numAdvts = 0;
  int egressPort = 0;
  struct vid_addr_tuple *current = NULL;
  if(treeNo == 1){
      current = primary_vid_tbl_head;
  }
  else{
      current = secondary_vid_tbl_head;
  }

  // Port from where VID request came.
  int i;

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

		//sending VID out of the same port with the same VID so the upstream MTS knows it can add it to their CPVID table
    if (strncmp(interface, current->eth_name, strlen(interface)) == 0)
		{
      sprintf(vid_addr, "%s", current->vid_addr);
    }

		else
		{
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
int build_JOIN_MSG_PAYLOAD(uint8_t *data, int treeNo)
{
  int payloadLen = 0;

  // <MSG_TYPE> - Hello Join Message, Type - 1.
  data[payloadLen] = (uint8_t) MTP_TYPE_JOIN_MSG;

  // next byte
  payloadLen = payloadLen + 1;

  data[payloadLen] = (uint8_t) treeNo;

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

// Message ordering <MSG_TYPE>
int  build_PERIODIC_MSG_PAYLOAD(uint8_t *data)
{
  int payloadLen = 0;

  // <MSG_TYPE> - Hello Periodic Message, Type - 2.
  data[payloadLen] = (uint8_t) MTP_TYPE_PERODIC_MSG;

  // next byte
  payloadLen = payloadLen + 1;

  // Return the total payload Length.
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
 *
 */

// Message ordering <MSG_TYPE> <OPERATION> <NUMBER_VIDS> <VID_ADDR_LEN> <MAIN_TABLE_VID + EGRESS PORT>
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
 *   isPrimary_VID_Table_Empty -     Check if Main VID Table is empty.
 *
 *   @input
 *   no params
 *
 *   @return
 *   true  	- if Main VID Table is empty.
 *   false 	- if Main VID Table is not null.
 */

// Message ordering <MSG_TYPE>
bool isPrimary_VID_Table_Empty()
{
	if (primary_vid_tbl_head)
	{
		return false;
	}

  return true;
}


/*
 *   isSecondary_VID_Table_Empty -     Check if Main VID Table is empty.
 *
 *   @input
 *   no params
 *
 *   @return
 *   true  	- if Main VID Table is empty.
 *   false 	- if Main VID Table is not null.
 */

// Message ordering <MSG_TYPE>
bool isSecondary_VID_Table_Empty()
{
    if (secondary_vid_tbl_head)
    {
        return false;
    }

    return true;
}

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
		//------------------------convergence info---------------------------------
		clock_gettime(CLOCK_MONOTONIC_RAW, &end);
		uint64_t convergenceTime = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
		FILE *f = fopen("convergenceTime.txt", "a");
		if (f == NULL)
		{
    	printf("Error opening file!\n");
		}
		fprintf(f, "%" PRIu64 "\n", convergenceTime);
		fclose(f);
		system("date +%H:%M:%S:%N >> convergenceTime.txt");
		//-------------------------------------------------------------------------

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
			while(current!=NULL && (current->path_cost < node->path_cost))
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
				// Update time stamp.
				current->last_updated = time(0);
				return true;
			}
			current = current->next;
		}
	}
	return false;
}


/**
 *		Print VID Table.
 *      	VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		void
**/
void print_entries_LL()
{
	struct vid_addr_tuple *current;

  int tracker = MAX_MAIN_VID_TBL_PATHS;
	int lenCurrentVID;

	printf("\n%s\n", "#######Primary Main VID Table#########");
	printf("%s %30s %10s %10s %5s\n","| MT_VID |", "| Interface Name |", "| Path Cost |", "| Membership |", "| Source MAC Address |");

	for (current = primary_vid_tbl_head; current != NULL && current->membership <= MAX_MAIN_VID_TBL_PATHS; current = current->next)
	{
		lenCurrentVID = strlen(current->vid_addr);
		printf("%*s %*s %15d %13d %29s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac));
	}
}


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
    if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0)
		{
      current->last_updated = time(0);
      hasUpdates = true;
    }
  }
  return hasUpdates;
}


/**
 *              Check for link Failures.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              void
**/
int checkForFailuresPrimary(char **deletedVIDs)
{
  struct vid_addr_tuple *current = primary_vid_tbl_head;
  struct vid_addr_tuple *previous = NULL;
  time_t currentTime = time(0);
  int numberOfFailures = 0;

  while (current != NULL)
	{
    if ((current->last_updated !=-1) && (currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME))
		{
			/* CONTROL INFORMATION COLLECTION FOR CONVERGENCE */
			system("echo -n `date +\"FAILED VID'S WERE REMOVED AT [%H:%M:%S:%6N]\"` >> linkFail.txt");
			char eth[25];
			sprintf(eth, "echo -n ' [%s]' >> linkFail.txt", current->eth_name);
			system(eth);
			char vidToDelete[70];
			sprintf(vidToDelete, "echo ' [%s]' >> linkFail.txt", current->vid_addr);
			system(vidToDelete);
			/* CONTROL INFORMATION COLLECTION FOR CONVERGENCE */

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
				system("echo -n `date +\"VID REMOVED AT [%H:%M:%S:%6N] \"` >> linkFail.txt");
				char vidToDelete[100];
				sprintf(vidToDelete, "echo ' [%s]' >> linkFail.txt", current->vid_addr);
				system(vidToDelete);
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


/**
 *    Print VID Table.
 *    Backup VID Paths,    Implemented Using linked list, instead of maintaining a seperate table, I am adding Main VIDS and Backup Paths
 *                         in the same table.
 *    Head Ptr,   *vid_table
 *
 *    @return
 *    void
**/
void print_entries_bkp_LL()
{
	struct vid_addr_tuple *current;

	int lenCurrentVID;

	printf("\n%s\n", "####### Primary Backup VID Table #########");
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
int add_entry_LL2(struct vid_addr_tuple *node)
{
    struct vid_addr_tuple *current = secondary_vid_tbl_head;
    bool VIDpruning = false;

    int tracker = 0;
    if (!find_entry_LL2(node))
    {
        //------------------------convergence info---------------------------------
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        uint64_t convergenceTime = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
        FILE *f = fopen("convergenceTime.txt", "a");
        if (f == NULL)
        {
            printf("Error opening file!\n");
        }
        fprintf(f, "%" PRIu64 "\n", convergenceTime);
        fclose(f);
        system("date +%H:%M:%S:%N >> convergenceTime.txt");
        //-------------------------------------------------------------------------

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
            while(current!=NULL && (current->path_cost < node->path_cost))
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
            }

            // Increment the membership IDs of other VID's
            while (current != NULL)
            {
                current->membership++;
                current = current->next;
            }

            if(sizeOfSecondaryVIDTable() > MAX_VID_ENTRIES)
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


/**
 *		Check if the VID entry is already present in the table.
 *      VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		true			Element Found.
 *		false			Element Not Found.
**/
bool find_entry_LL2(struct vid_addr_tuple *node)
{
    if (secondary_vid_tbl_head != NULL)
    {
        struct vid_addr_tuple *current = secondary_vid_tbl_head;
        while (current != NULL)
        {
            if (strcmp(current->vid_addr, node->vid_addr) == 0)
            {
                // Update time stamp.
                current->last_updated = time(0);
                return true;
            }
            current = current->next;
        }
    }
    return false;
}


/**
 *		Print VID Table.
 *      	VID Table,		Implemented Using linked list.
 * 		Head Ptr,		*vid_table
 *
 *		@return
 *		void
**/
void print_entries_LL2()
{
    struct vid_addr_tuple *current;

    int tracker = MAX_MAIN_VID_TBL_PATHS;
    int lenCurrentVID;

    printf("\n%s\n", "####### Secondary Main VID Table#########");
    printf("%s %30s %10s %10s %5s\n","| MT_VID |", "| Interface Name |", "| Path Cost |", "| Membership |", "| Source MAC Address |");

    for (current = secondary_vid_tbl_head; current != NULL && current->membership <= MAX_MAIN_VID_TBL_PATHS; current = current->next)
    {
        lenCurrentVID = strlen(current->vid_addr);
        printf("%*s %*s %15d %13d %29s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->eth_name, current->path_cost, current->membership, ether_ntoa(&current->mac));
    }
}


/**
 *              Update timestamp for a MAC address on both VID Table and backup table.
 *     		VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              bool
**/
bool update_hello_time_LL2(struct ether_addr *mac)
{
    struct vid_addr_tuple *current;
    bool hasUpdates = false;

    for (current = secondary_vid_tbl_head; current != NULL; current = current->next)
    {
        if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0)
        {
            current->last_updated = time(0);
            hasUpdates = true;
        }
    }
    return hasUpdates;
}


/**
 *              Check for link Failures.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              void
**/
int checkForFailuresSecondary(char **deletedVIDs)
{
    struct vid_addr_tuple *current = secondary_vid_tbl_head;
    struct vid_addr_tuple *previous = NULL;
    time_t currentTime = time(0);
    int numberOfFailures = 0;

    while (current != NULL)
    {
        if ((current->last_updated !=-1) && (currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME))
        {
            /* CONTROL INFORMATION COLLECTION FOR CONVERGENCE */
            system("echo -n `date +\"FAILED VID'S WERE REMOVED AT [%H:%M:%S:%6N]\"` >> linkFail.txt");
            char eth[25];
            sprintf(eth, "echo -n ' [%s]' >> linkFail.txt", current->eth_name);
            system(eth);
            char vidToDelete[70];
            sprintf(vidToDelete, "echo ' [%s]' >> linkFail.txt", current->vid_addr);
            system(vidToDelete);
            /* CONTROL INFORMATION COLLECTION FOR CONVERGENCE */

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
 *              Delete Entries in the vid_table using vid as a reference.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              true - if deletion in main VID table occurs (should it do the backup table?)
 *              false - if deletion fails.
**/
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
                system("echo -n `date +\"VID REMOVED AT [%H:%M:%S:%6N] \"` >> linkFail.txt");
                char vidToDelete[100];
                sprintf(vidToDelete, "echo ' [%s]' >> linkFail.txt", current->vid_addr);
                system(vidToDelete);
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
 *              Get Instance of Main VID Table.
 *              VID Table,              Implemented Using linked list.
 *              Head Ptr,               *vid_table
 *
 *              @return
 *              struct Secondary_vid_tbl_head* - reference of main vid table.
**/
struct vid_addr_tuple* getInstance_vid_tbl_LL2()
{
    return secondary_vid_tbl_head;
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
void print_entries_bkp_LL2()
{
    struct vid_addr_tuple *current;

    int lenCurrentVID;

    printf("\n%s\n", "####### Secondary Backup VID Table#########");
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
 *    Add into the Child PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *Secondary_cpvid_tbl_head
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


/**
 *    Print Child PVID  Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *primary_cpvid_tbl_head
 *
 *    @return
 *    void
**/
void print_entries_cpvid_LL()
{
	struct child_pvid_tuple *current;

	int lenCurrentVID;

	printf("\n%s\n", "####### Primary Child PVID Table#########");
	printf("%s %26s %10s\n","| Child PVID |", "| Interface Name |", "| Source MAC Address |");

	for(current = primary_cpvid_tbl_head; current != NULL; current = current->next)
	{
		lenCurrentVID = strlen(current->vid_addr);
		printf("%*s %*s %30s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->child_port, ether_ntoa(&current->mac));
	}
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
			system("echo -n `date +\"CHILD VID REMOVED AT [%H:%M:%S:%6N] \"` >> linkFail.txt");
			char vidToDelete[70];
			sprintf(vidToDelete, "echo ' [%s]' >> linkFail.txt", current->vid_addr);
			system(vidToDelete);

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
    if (memcmp(mac, &current->mac, sizeof(struct ether_addr)) == 0)
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

  for (current = primary_cpvid_tbl_head; current != NULL; current = current->next) {
    if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0) {
      current->last_updated = time(0);
      isUpdated = true;
    }
  }
  return isUpdated;
}


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

  for (current = primary_cpvid_tbl_head; current != NULL; current = current->next)
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


/**
 *              Check for link Failures.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *primary_cpvid_tbl_head
 *
 *              @return
 *              void
**/
bool checkForFailuresPrimaryCPVID()
{
  struct child_pvid_tuple *current = primary_cpvid_tbl_head;
  struct child_pvid_tuple *previous = NULL;
  time_t currentTime = time(0);
  bool hasDeletions = false;

  while (current != NULL) {
    if ((current->last_updated !=-1) &&(currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME) ) {
      struct child_pvid_tuple *temp = current;
      if (previous == NULL) {
        primary_cpvid_tbl_head = current->next;
      } else {
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
 *    Add into the Child PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *secondary_cpvid_tbl_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/already exists.
**/
bool add_entry_cpvid_LL2(struct child_pvid_tuple *node)
{
    bool newAddition = false;

    if(secondary_cpvid_tbl_head != NULL)
    {
        bool updatedNode = update_entry_cpvid_LL(node);

        if(!updatedNode && !find_entry_cpvid_LL(node))
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
 *    Check if the VID entry is already present in the table.
 *    Child PVID Table,  Implemented Using linked list.
 *    Head Ptr,   *secondary_cpvid_tbl_head
 *    @return
 *    true      Element Found.
 *    false     Element Not Found.
**/
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
 *    Print Child PVID  Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *secondary_cpvid_tbl_head
 *
 *    @return
 *    void
**/
void print_entries_cpvid_LL2()
{
    struct child_pvid_tuple *current;

    int lenCurrentVID;

    printf("\n%s\n", "#######Secondary Child PVID Table#########");
    printf("%s %26s %10s\n","| Child PVID |", "| Interface Name |", "| Source MAC Address |");

    for(current = secondary_cpvid_tbl_head; current != NULL; current = current->next)
    {
        lenCurrentVID = strlen(current->vid_addr);
        printf("%*s %*s %30s\n", (lenCurrentVID + 2), current->vid_addr, (26 - lenCurrentVID), current->child_port, ether_ntoa(&current->mac));
    }
}


/**
 *    Get instance of child of PVID Table.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *secondary_cpvid_tbl_head
 *
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/
struct child_pvid_tuple* getInstance_cpvid_LL2()
{
    return secondary_cpvid_tbl_head;
}


/**
 *    Delete any Child PVID's matching this VID.
 *    Child PVID Table,    Implemented Using linked list.
 *    Head Ptr,   *secondary_cpvid_tbl_head
 *
 *    @input
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/
bool delete_entry_cpvid_LL2(char *cpvid_to_be_deleted)
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    bool hasDeletions = false;

    while (current != NULL)
    {
        if (strncmp(cpvid_to_be_deleted, current->vid_addr, strlen(cpvid_to_be_deleted)) == 0)
        {
            system("echo -n `date +\"CHILD VID REMOVED AT [%H:%M:%S:%6N] \"` >> linkFail.txt");
            char vidToDelete[70];
            sprintf(vidToDelete, "echo ' [%s]' >> linkFail.txt", current->vid_addr);
            system(vidToDelete);

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
 *    Head Ptr,   *secondary_cpvid_tbl_head
 *
 *    @input
 *    char * - cpvid to be deleted.
 *    @return
 *    struct child_pvid_tuple* - return reference of child pvid table.
**/
bool delete_MACentry_cpvid_LL2(struct ether_addr *mac)
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    bool hasDeletions = false;

    while (current != NULL)
    {
        if (memcmp(mac, &current->mac, sizeof(struct ether_addr)) == 0)
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
 *              Update timestamp for a MAC address on both CPVID Table and backup table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *secondary_cpvid_tbl_head
 *
 *              @return
 *              void
**/
bool update_hello_time_cpvid_LL2(struct ether_addr *mac)
{
    struct child_pvid_tuple *current;
    bool isUpdated = false;

    for (current = secondary_cpvid_tbl_head; current != NULL; current = current->next) {
        if (memcmp(&current->mac, mac, sizeof (struct ether_addr))==0) {
            current->last_updated = time(0);
            isUpdated = true;
        }
    }
    return isUpdated;
}


/**
 *              Update entry for a MAC address on both CPVID Table.
 *              Child PVID Table,       Implemented Using linked list.
 *              Head Ptr,               *secondary_cpvid_tbl_head
 *
 *              @return
 *              void
**/
bool update_entry_cpvid_LL2(struct child_pvid_tuple *node)
{
    struct child_pvid_tuple *current;

    for (current = secondary_cpvid_tbl_head; current != NULL; current = current->next)
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

bool checkForFailuresSecondaryCPVID()
{
    struct child_pvid_tuple *current = secondary_cpvid_tbl_head;
    struct child_pvid_tuple *previous = NULL;
    time_t currentTime = time(0);
    bool hasDeletions = false;

    while (current != NULL) {
        if ((current->last_updated !=-1) &&(currentTime - current->last_updated) > (3 * PERIODIC_HELLO_TIME) ) {
            struct child_pvid_tuple *temp = current;
            if (previous == NULL) {
                secondary_cpvid_tbl_head = current->next;
            } else {
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
 *    Add into the local host broadcast Table.
 *    Local Host broadcast Table, Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      Successful Addition
 *    false     Failure to add/ Already exists.
**/
bool add_entry_lbcast_LL(struct local_bcast_tuple *node)
{
  if (local_bcast_head == NULL) {
    local_bcast_head = node;
  } else {
    if (!find_entry_lbcast_LL(node)) {
      node->next = local_bcast_head;
      local_bcast_head = node;
    }
  }
}

/**
 *    Find entries in the local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true      If element is found.
 *    false     If element is not found.
**/

bool find_entry_lbcast_LL(struct local_bcast_tuple *node) {
  struct local_bcast_tuple *current = local_bcast_head;

  if (current != NULL) {

    while (current != NULL) {

      if (strcmp(current->eth_name, node->eth_name) == 0) {
        return true;
      }

      current = current->next;
    }
  }

  return false;
}

/**
 *    Print local host broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    void
**/
void print_entries_lbcast_LL()
{
  struct local_bcast_tuple *current;
	printf("\n%s\n", "#######Local Host Broadcast Table#########");
	printf("%s\n", "| Interface Name |");

  for (current = local_bcast_head; current != NULL; current = current->next)
	{
    printf("%6s\n", current->eth_name);
  }
}


/**
 *    Delete a port from local broadcast Table.
 *    Local Host broadcast Table,    Implemented Using linked list.
 *    Head Ptr,   *local_bcast_head
 *    @return
 *    true  - if deletion successful.
 *    false - if deletion not successful.
**/

bool delete_entry_lbcast_LL(char *port) {
  struct local_bcast_tuple *current = local_bcast_head;
  struct local_bcast_tuple *previous = NULL;
  bool isPortDeleted = false;

  while (current != NULL) {
    if (strcmp(current->eth_name, port) == 0) {
      if (current == local_bcast_head) {
        local_bcast_head = current->next;
        free(current);
        current = local_bcast_head;
        previous = NULL;
        continue;
      } else {
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

//PETER FUNCTIONS:
//make sure to add a functional prototype in feature_payload.h FOR ALL ADDED FUNCTIONS!!!!

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
/*
int checkForMainVIDTableChanges(char **VIDs, char **deletedVIDs)
{
	struct vid_addr_tuple *current;
	int numberOfFailures = 0;
	bool noVIDMatches = true;
	int i = 0;
	int k = 0;

	for (current = primary_vid_tbl_head; current->membership < 4; current = current->next)
	{
		//when there are more than 1 deletion, this needs to be reset..
		noVIDMatches = true;

		for(i = 0; i < 3; i++)
		{
			printf("comparing %s and %s \n", current->vid_addr, VIDs[i]);

			if ((strcmp(VIDs[i], current->vid_addr)) == 0)
			{
				noVIDMatches = false;
				break;
			}
		}

		if(noVIDMatches)
		{
			deletedVIDs[numberOfFailures] = (char*)calloc(strlen(current->vid_addr), sizeof(char));
			strncpy(deletedVIDs[numberOfFailures], current->vid_addr, strlen(current->vid_addr));

			printf("New VID (from deletedVIDs): %s\n", deletedVIDs[numberOfFailures]);
			numberOfFailures++;
		}
	}
	return numberOfFailures;
}
*/

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

//void printPDUInfo() {}
