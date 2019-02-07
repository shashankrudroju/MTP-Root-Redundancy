#!/usr/bin/env python
#Description: Used to clean up and display what was printed to the console from an MTS during MTP's operation
#Author: Peter Willis (pjw7904@rit.edu)
import sys

fileName = sys.argv[1]
tableOutput = []
tableUpdates = []
bcastDestinations = {}
broadcastCount = 0
inTable = False

with open(fileName, "r") as readFile:
    for line in readFile:
        if(line.strip()):
            if("Received broadcast frame" in line):
                broadcastCount += 1

            elif("Sent to" in line):
                if line in bcastDestinations:
                    bcastDestinations[line] = (bcastDestinations[line] + 1)
                else:
                    bcastDestinations[line] = 1

            elif("---" in line):
                inTable = not inTable

                if(inTable == False):
                    tableUpdates.append(tableOutput[:])
                    #clear list
                    tableOutput[:] = []

            if(inTable):
                tableOutput.append(line.rstrip())


print("{} broadcast frames were switched at this MTS".format(broadcastCount))

for key,val in bcastDestinations.items():
    print("\tBroadcast frames were {} {} times".format(key.rstrip(),val))

for element in tableUpdates[len(tableUpdates) - 1]:
    print element

print("----------------------------------------------------------")
