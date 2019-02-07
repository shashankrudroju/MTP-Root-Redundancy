# The Meshed Tree Protocol (MTP)
IEEE Project 1910.1 - Standard for Meshed Tree Bridging with Loop Free Forwarding

Loop avoidance is essential in switched networks to avoid broadcast storms. Logical Spanning Trees are constructed on the physical meshed topologies to overcome the broadcast storm problem. However, during topology changes, frame forwarding latency is introduced when re-converging and identifying new spanning tree paths. Meshed Tree algorithms (MTA) offer a new approach. Meshed Trees support multiple tree branches from a single root to cut down on re-convergence latency on link failures. A Meshed Tree Protocol (MTP) based on the MTA is currently under development in this repository.

### Requirements to Run the MTP Implementation
1. A Unix system with multiple NICs (preferably Ubuntu 16.04, that is what it is currently being tested on)
2. The GNU Compiler Collection (GCC)
3. Python 2.7 (If you need to use the automation script)

### How to Run the Implementation Manually
*These commands assume you are in the MTP directory on the system*

1. To start a node as the root:
**bin/mtpd 1 1**

2. To start a node as a non-root:
**bin/mtpd 0**

### How to Run the Implementation Via Automation
1. Clone the MTP-Automation repository (if you don't have access, ask someone on the project)
2. Modify **MTPStart.py** with your credentials
3. Update **MTP_RSPEC.xml** With your GENI RSPEC
4. Update **cmd_file.txt** with the names of your MTP nodes and their function (1 = root | 0 = non-root)
5. Run **MTPStart.py** and follow the prompts (Generally, the Root node should be started 5~ seconds before the rest for testing purposes)
6. Run any of the various collection scripts to gather logs from the MTP nodes
7. Run **MTPStop.py** to stop the implementation (mtpd) from running
