CS 438 Communication Networks
MP2 Implementation of Unicast Routing Protocols
Kuang-Huei Lee (klee156)
Meng Zhang (mzhang61)


1. File Included
----------------
manager            executable on manager side
distvec            executable on node side, using distance vector protocol
linkstate          executable on node side, using link state protocol
manager.cpp        source file of manager
distvec.cpp        source file of distvec_node
linkstate.cpp      source file of linkstate_node
Makefile           Makefile to compile manager.cpp, distvec.cpp and linkstate.cpp
topoA.txt	   topology test file A
topoB.txt	   topology test file B
msgA.txt	   message test file A
msgb.txt	   message test file B
Readme.txt         readme document
MP2_Report.docx    Report Document


2. Commandline Arguments
------------------------

A. manager
   
   ./manager [topologyfile] [messagefile]
   Example: ./manager topoA.txt msgA.txt

B. distance vector node
   
   ./distvec [managerhostname]
   Example: ./distvec localhost
            ./distvec remlnx.ews.illinois.edu

C. link state node
   
   ./linkstate [managerhostname]
   Example: ./linkstate localhost
            ./linkstate remlnx.ews.illinois.edu


3. Design
---------

A. Manager

   The manager program reads a topology file and a message file and provides corresponding information to its connection nodes separately. The manager also allows the new update of topology information. When launching the manager, it will wait for the connection of nodes. The following action may happens: 
   (1) Once a node is connected: assign a new ID for the coming node and provide the information of the node’s neighbour, including the  ID, path cost, IP address etc.
   (2) Once there is an update of topology, send the updated topology information to the relevant nodes. 
       The updating may include: add/disconnect path, add/remove node, change pathcost   
   (3) Once all the nodes have converged, manager begins to send message according to the message file.
   

B. Distance Vector Node
   
   The Node use distance vector protocol to set up its routing table. When the node connects to manager and gets path information, it will print out accordingly, eg. “now linked to node 2 with cost 5” or “no longer linked to node 3”. When it converge, the node send the convergence information to manager and print out its routing table so far: <destination> <nexthop> <pathcost>.
   The Node may get the message from either manager or its neighbour. The message contains the information of source, destination and text. According to the routing table and destination, the node will send this message to the next node. The node will print out message and so far path.


C. Link State Node

   The Node use link state protocol to set up its routing table. When the node connects to manager and gets path information, it will print out accordingly, eg. “now linked to node 3 with cost 1” or “no longer linked to node 4”. When it converge, the node send the convergence information to manager and print out its routing table so far: <destination> <pathcost> <shortest path: hop1 hop2 hop3…>
   The Node may get the message from either manager or its neighbour. The message contains the information of source, destination and text. According to the routing table and destination, the node will send this message to the next node. The node will print out message and so far path.


