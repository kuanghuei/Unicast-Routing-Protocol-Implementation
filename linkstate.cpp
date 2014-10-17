/*
** distvec.c -- distance vector client
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <time.h>       /* time */
#include <fstream>
#include <pthread.h>
#include <vector>

#define PORT "3490"  // the port users will be connecting to
#define PORTNUM 3490
#define MAXDATASIZE 100
#define MAXATTEMPTS 8
#define BACKLOG 10   // how many pending connections queue will hold
#define MAX_NODE_NUM 16
#define MAX_MSG_NUM 20
#define MAX_MSG_SIZE 200

using namespace std;

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

string my_to_string(int a){
    stringstream ss;
    ss << a;
    string str = ss.str();
    return str;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct {
    int id;
    int cost;
    int nexthop;
    int lasthop;
    string track;
} hnode;

int heapSize;
int initial_node_num;
string IP_table[MAX_NODE_NUM];


void swap(hnode A[], int i, int j){
    int tempid = A[i].id;
    int tempcost = A[i].cost;
    int tempnexthop = A[i].nexthop;
    int templasthop = A[i].lasthop;
    string temptrack = A[i].track;
    A[i].id = A[j].id;
    A[i].cost = A[j].cost;
    A[i].nexthop = A[j].nexthop;
    A[i].lasthop = A[j].lasthop;
    A[i].track = A[j].track;

    A[j].id = tempid;
    A[j].cost = tempcost;
    A[j].nexthop = tempnexthop;
    A[j].lasthop = templasthop;
    A[j].track = temptrack;
}
    
void minHeapify(hnode A[], int i){
    int l = 2*i;
    int r = 2*i + 1;
    int smallest = 0;
    
    smallest = (l < heapSize && A[l].cost < A[i].cost) ? l:i;
    smallest = (r < heapSize && A[r].cost < A[smallest].cost) ? r:smallest;
    
    if (smallest != i) {
        swap(A, smallest, i);
        minHeapify(A, smallest);
    }
}

void initTopology(int topology[][MAX_NODE_NUM]){
    initial_node_num = 0;
    for(int i=0;i<MAX_NODE_NUM;i++) {
        for(int j=0;j<MAX_NODE_NUM;j++) {
            if(i==j)
                topology[i][j]=0;
            else
                topology[i][j]=-1;
        }
    }

    for(int i=0;i<MAX_NODE_NUM; i++){
        IP_table[i] = "";
    }
}

void removeNode(hnode A[], int i){
    swap(A, i, heapSize - 1);
    --heapSize;
    minHeapify(A, i);
}

void buildHeap(hnode A[]){
    for (int i = heapSize/2; i >= 0; --i)
        minHeapify(A, i);
}

void dijkstra(int topomap[][MAX_NODE_NUM], hnode routingtable[], int src){
    //calculate how many nodes are there
    hnode heapD[MAX_NODE_NUM];
    //path to itself
    routingtable[src-1].id = src;
    routingtable[src-1].cost = 0;
    routingtable[src-1].nexthop = src;
    routingtable[src-1].lasthop = src;
    routingtable[src-1].track = my_to_string(src);

    heapSize = 0;
    for (int i=0; i < MAX_NODE_NUM; ++i){
        heapD[i].id = i+1;
        heapD[i].nexthop = i+1;
        heapD[i].lasthop = i+1;
        if (topomap[src-1][i] == -1){
            heapD[i].cost = 1000; //INT_MAX
        } else {
            heapD[i].cost = topomap[src-1][i];
            heapD[i].track = my_to_string(src);
            if (heapD[i].cost > 0){
                heapD[i].track+= " ";
                heapD[i].track+= my_to_string(i+1);
            }
            //cout<<heapD[i].track<<"\n";
        }
        ++heapSize;
    }

    //create a heap to store D(v)   
    buildHeap(heapD);
    removeNode(heapD, 0);
    // for (int i=0; i< heapSize; ++i){
    //     cout<<heapD[i].id<<" "<<heapD[i].cost<<": "<<heapD[i].track<<"\n";
    // }
    // printf("KERKER%d\n", heapSize);
    int n = 1;
    while (heapSize > 0){
        //find w not in N' such that D(w) is a minimum
        //add w to N
        int id = heapD[0].id;
        //cout<<"ID "<<id<<"\n";
        int cost = heapD[0].cost;
        int nexthop = heapD[0].nexthop;
        int lasthop = heapD[0].lasthop;
        string track = heapD[0].track;
        //printf("%d, %d, %d\n", id, cost, nexthop);
        routingtable[heapD[0].id-1].id = id;
        routingtable[heapD[0].id-1].cost = cost;
        routingtable[heapD[0].id-1].nexthop = nexthop;
        routingtable[heapD[0].id-1].lasthop = lasthop;
        routingtable[heapD[0].id-1].track = track;
        removeNode(heapD, 0);
        
        //update D(v) for all v adjacent to w and not in N' : 
        //D(v) = min( D(v), D(w) + c(w,v) ) 
        for (int i=0; i < heapSize; ++i){
            if (topomap[heapD[i].id-1][id-1] > 0){
                if (heapD[i].cost > cost + topomap[heapD[i].id-1][id-1]){
                    heapD[i].cost = cost + topomap[heapD[i].id-1][id-1];
                    heapD[i].nexthop = nexthop;
                    heapD[i].lasthop = id;
                    heapD[i].track = track;
                    heapD[i].track+= " ";
                    heapD[i].track+= my_to_string(heapD[i].id);   
                }
            }
            buildHeap(heapD);
        }
        //++n;
    }
    for (int i=0; i<MAX_NODE_NUM; ++i){
        if (routingtable[i].cost >= 1000)
            routingtable[i].cost = -1;
    }
    // printf("KERKER%d\n", heapSize);
}


int main(int argc, char *argv[]){

    if (argc != 2) {
        fprintf(stderr,"[usage] hostname\n");
        exit(1);
    }

    int id_max = 0;
    int topomap[MAX_NODE_NUM][MAX_NODE_NUM];
    initTopology(topomap);
    int sockfd, listenerfd, numbytes;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buf[256];
    int neighbors_addr_len;
    int received_from[MAX_NODE_NUM];
    memset(received_from, 0, MAX_NODE_NUM);
    int flooded = 0;
    //string bufstr;
    string neighbors = "", self = "";

    //init routingtable
    hnode routingtable[MAX_NODE_NUM];
    for (int i = 0; i < MAX_NODE_NUM; ++i){
        routingtable[i].id = 0;
        routingtable[i].cost = 1000;
        routingtable[i].nexthop = 0;
        routingtable[i].lasthop = 0;
        string track = "";
    }

    int ID, nID;
    if (argc != 2) {
        fprintf(stderr,"usage: client-hostname server-port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                        s, sizeof s);
    //printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    memset(buf, 0, sizeof(buf));

    numbytes = recv(sockfd, buf, sizeof(buf), 0);
    if (numbytes < 0) {
        perror("recv");
        exit(1);
    } 
    
    buf[numbytes] = '\0';
    string bufstr(buf);
    //cout<<bufstr<<'\n';
    vector<string> bufv = split(bufstr, ' ');
    ID = stoi(bufv[0]);
    bufv.erase(bufv.begin());

    

    while(!bufv.empty()){
        nID = stoi(bufv[0]);
        topomap[ID-1][nID-1] = stoi(bufv[1]); //cost
        topomap[nID-1][ID-1] = stoi(bufv[1]);
        IP_table[nID-1] = bufv[2];
        //cout<<rt[nID-1].id<<" "<<rt[nID-1].nexthop<<" "<<rt[nID-1].cost<<" "<<rt[nID-1].ipstr<<'\n';
        cout<<"now linked to node "<<nID<<" with cost "<<stoi(bufv[1])<<"\n";
        //flood the link
        bufv.erase(bufv.begin(), bufv.begin()+3);
    }

    //setup UDP listener
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    p = NULL;
    //cout<<"this port "<<my_to_string(PORTNUM + ID)<<"\n";

    if ((rv = getaddrinfo(NULL, my_to_string(PORTNUM + ID).c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((listenerfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(listenerfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(listenerfd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }


    //flood topology
    string link = "";
    for (int i=0; i<MAX_NODE_NUM; ++i){
        if (topomap[ID-1][i] > 0){
            link+= my_to_string(ID);
            link+= " ";
            link+= my_to_string(i+1); //nID
            link+= " ";
            link+= my_to_string(topomap[ID-1][i]); //cost
            link+= " ";
        }
    }
    flooded=1;
    received_from[ID-1]=1;
    for (int i=0; i<MAX_NODE_NUM; ++i){
        if (topomap[ID-1][i] > 0){
            struct sockaddr_in neighbors_addr;
            bzero(&neighbors_addr,sizeof(neighbors_addr));
            neighbors_addr.sin_family=AF_INET;
            neighbors_addr.sin_port=htons(PORTNUM+i+1);
            neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
            neighbors_addr_len = sizeof neighbors_addr;
            if ((numbytes=sendto(listenerfd, link.c_str(), strlen(link.c_str()), 
                0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                perror("send error");
            //cout<<"Sending "<<numbytes<<" bytes "<<routing_vector<<" to Node "<< (i+1)<<"\n";
        }
    }

    //dijkstra(topomap, routingtable, ID);

    fd_set master, read_fds;    // master file descriptor list
    FD_ZERO(&master);    // clear the master and temp sets
    FD_SET(sockfd,&master); /* add STDIN to connset */
    FD_SET(listenerfd, &master); /* add listenerfd to connset */
    int fdmax = listenerfd;
    int convergence = 0;
    struct timeval tv;

    int convergetime = 2;
    tv.tv_sec = convergetime;
    tv.tv_usec = 0;
    int defaulttimeout = 200000;
    memset(received_from, 0, sizeof(received_from));

    //while loop
    while(1) {  // main accept() loop
        //printf("Complete\n");
        //cout<<"check\n";
        read_fds = master;
        int selectret;
        if((selectret = select(fdmax+1,&read_fds,NULL,NULL,&tv)) < 0){
            fprintf(stdout, "select() error\n");
            exit(4);
        }
        //converged
        if (selectret == 0 && convergence == 0 && tv.tv_sec == convergetime){
            memset(received_from, 0, sizeof(received_from));
            flooded = 0;
            // for (int i=0; i<MAX_NODE_NUM; ++i){
            //     for (int j=0; j<MAX_NODE_NUM; ++j){
            //         cout<<topomap[i][j]<<" ";                
            //     }
            //     cout<<"\n";
            // }
            
            //cout<<"Converged!\n";
            // dijkstra(topomap, routingtable, ID);
            // for (int i=0; i< MAX_NODE_NUM; ++i){
            //     cout<<routingtable[i].id<<" "<<routingtable[i].cost<<": "<<routingtable[i].track<<"\n";
            // }
            convergence = 1;
            tv.tv_sec = defaulttimeout;
            if (send(sockfd, "c", 2, 0) == -1)
                perror("send error");
            // for (int i=0; i<MAX_NODE_NUM; ++i){
            //     cout<<received_from[i]<<" ";
            // }
            //cout<<"////////////////////////////////////\n";
        }

        //cout<<"still waiting...";
        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                //message from nodes
                if (i == listenerfd){
                    //cout<<"listenerfd\n";
                    //maybe squeezed into one packet

                    memset(buf, 0, sizeof(buf));
                    struct sockaddr_in node_addr;
                    socklen_t node_len;
                    int numbytes;
                    if ((numbytes = recvfrom(listenerfd, buf, 256, 0, (struct sockaddr*)&node_addr, &node_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }
                    //parse
                    string bufstr2(buf); 
                    vector<string> bufv2 = split(bufstr2, ' ');
                    //cout<<bufstr2<<"\n";
                    //cout<<"Get "<<numbytes<<" from neighbor "<<bufstr2<<"\n";
                    //if message
                    if (bufv2[0] == "from"){
                        cout<<bufstr2<<"\n"; //display message
                        if (stoi(bufv2[3]) == ID);
                        else if (routingtable[stoi(bufv2[3])-1].cost == -1){
                            cout<<"There is no route to "<<bufv2[3]<<"\n";
                        }
                        else {
                            int nexthop = routingtable[stoi(bufv2[3])-1].nexthop;
                            string msgtosend = "from ";
                            msgtosend+= bufv2[1];
                            msgtosend+= " to ";
                            msgtosend+= bufv2[3];
                            msgtosend+= " hops ";
                            int m = 5;
                            while(bufv2[m] != "message"){
                                msgtosend+= bufv2[m++];
                                msgtosend+= " ";
                            }
                            msgtosend+= my_to_string(nexthop);
                            msgtosend+= " message ";
                            m++;

                            while(m < bufv2.size()){
                                msgtosend+= bufv2[m++];
                                msgtosend+= " ";
                            }

                            struct sockaddr_in neighbors_addr;
                            bzero(&neighbors_addr,sizeof(neighbors_addr));
                            neighbors_addr.sin_family=AF_INET;
                            neighbors_addr.sin_port=htons(PORTNUM+nexthop);
                            neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[nexthop-1].c_str()); 
                            neighbors_addr_len = sizeof neighbors_addr;
                            if ((numbytes=sendto(listenerfd, msgtosend.c_str(), strlen(msgtosend.c_str()), 
                                 0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                 perror("send error");
                            //cout<<msgtosend<<"\n";
                        }
                    }
                    else {
                        // cout<<"checkpoint2\n";
                        // for (int i=0; i<MAX_NODE_NUM; ++i){
                        //     cout<<received_from[i]<<" ";
                        // }
                        convergence = 0;
                        tv.tv_sec = convergetime;
                        int src, n1, n2, cost;
                        int stop = 0;
                        string link = bufstr2;
                        //cout<<"bufstr2 "<<bufstr2<<"\n";
                        src = stoi(bufv2[0]);
                        if (received_from[src-1] == 0){
                            received_from[src-1] = 1;
                            while(!bufv2.empty()){
                                n1 = stoi(bufv2[0]);
                                n2 = stoi(bufv2[1]);
                                cost = stoi(bufv2[2]);
                                //cout<<"n1: "<<n1<<" n2: "<<n2<<" cost: "<<cost<<"\n";
                                if (topomap[n1-1][n2-1] != cost){
                                    topomap[n1-1][n2-1] = cost;
                                    topomap[n2-1][n1-1] = cost;
                                }
                                bufv2.erase(bufv2.begin(), bufv2.begin()+3);
                                for (int i=0; i<MAX_NODE_NUM; ++i){
                                    if (topomap[ID-1][i] > 0){
                                        struct sockaddr_in neighbors_addr;
                                        bzero(&neighbors_addr,sizeof(neighbors_addr));
                                        neighbors_addr.sin_family=AF_INET;
                                        neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                        neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                        neighbors_addr_len = sizeof neighbors_addr;
                                        if ((numbytes=sendto(listenerfd, bufstr2.c_str(), strlen(bufstr2.c_str()), 
                                            0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                            perror("send error");
                                        //cout<<"Resending "<<numbytes<<" bytes "<<link<<" to Node "<< (i+1)<<"\n";
                                    }
                                }

                                if (flooded==0){
                                    flooded = 1;
                                    received_from[ID-1] = 1;
                                    string link = "";
                                    for (int i=0; i<MAX_NODE_NUM; ++i){
                                        if (topomap[ID-1][i] > 0){
                                            link+= my_to_string(ID);
                                            link+= " ";
                                            link+= my_to_string(i+1); //nID
                                            link+= " ";
                                            link+= my_to_string(topomap[ID-1][i]); //cost
                                            link+= " ";
                                        }
                                    }
                                    for (int i=0; i<MAX_NODE_NUM; ++i){
                                        if (topomap[ID-1][i] > 0){
                                            struct sockaddr_in neighbors_addr;
                                            bzero(&neighbors_addr,sizeof(neighbors_addr));
                                            neighbors_addr.sin_family=AF_INET;
                                            neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                            neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                            neighbors_addr_len = sizeof neighbors_addr;
                                            if ((numbytes=sendto(listenerfd, link.c_str(), strlen(link.c_str()), 
                                                0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                                perror("send error");
                                            //cout<<"triggerdflooding "<<numbytes<<" bytes "<<link<<" to Node "<< (i+1)<<"\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                //recv from manager
                else if (i == sockfd) {
                    //cout<<"sockfd\n";

                    memset(buf, 0, sizeof(buf));
                    numbytes = recv(sockfd, buf, sizeof(buf), 0);
                    if (numbytes < 0) {
                        perror("recv");
                        exit(1);
                    }
                    else if (numbytes == 0){
                    //    cout<<"end of connection\n";
                        exit(1);
                    }

                    //parse
                    string bufstr2(buf); 
                    vector<string> bufv2 = split(bufstr2, ' ');
                    //cout<<"check"<<bufstr2<<"\n";
                    //if message from manager
                    if (bufv2[0] == "from"){
                        cout<<bufstr2<<"\n"; //display message
                        if (stoi(bufv2[3]) == ID); //reach destination
                        else if (routingtable[stoi(bufv2[3])-1].cost == -1){
                            //cout<<"There is no route to "<<bufv2[3]<<"\n";
                        }
                        else {
                            int nexthop = routingtable[stoi(bufv2[3])-1].nexthop;
                            string msgtosend = "from ";
                            msgtosend+= bufv2[1];
                            msgtosend+= " to ";
                            msgtosend+= bufv2[3];
                            msgtosend+= " hops ";
                            int m = 5;
                            while(bufv2[m] != "message"){
                                msgtosend+= bufv2[m++];
                                msgtosend+= " ";
                            }
                            msgtosend+= my_to_string(nexthop);
                            msgtosend+= " message ";
                            m++;
                            while(m < bufv2.size()){
                                msgtosend+= bufv2[m++];
                                msgtosend+= " ";
                            }

                            struct sockaddr_in neighbors_addr;
                            bzero(&neighbors_addr,sizeof(neighbors_addr));
                            neighbors_addr.sin_family=AF_INET;
                            neighbors_addr.sin_port=htons(PORTNUM+nexthop);
                            //cout<<"nexthop "<<nexthop<<"\n";
                            neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[nexthop-1].c_str()); 
                            neighbors_addr_len = sizeof neighbors_addr;
                            if ((numbytes=sendto(listenerfd, msgtosend.c_str(), strlen(msgtosend.c_str()), 
                                 0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                 perror("sendto error");
                            //cout<<"Sending "<<msgtosend<<"\n";
                        }
                    } else if (bufv2[0] == "cvg") {
                        //cout<<bufv2[0]<<"\n";
                        dijkstra(topomap, routingtable, ID);
                        for (int i=0; i< MAX_NODE_NUM; ++i){
                            if (routingtable[i].cost != -1){
                                cout<<routingtable[i].id<<" "<<routingtable[i].cost<<": "<<routingtable[i].track<<"\n";
                            }
                        }
                    } else if (bufv2[0] == "recvg"){
                        convergence = 0;
                        tv.tv_sec = convergetime;
                    }
                    //topology change
                    else { 
                        convergence = 0;
                        tv.tv_sec = convergetime;
                        sleep(1);
                        //cout<<"Get "<<numbytes<<" from manager "<<bufstr2<<"\n";
                        nID = stoi(bufv2[1]);
                        int cost = stoi(bufv2[2]);
                        string ipstr = bufv2[3];
                        //update D table
                        //[destination][next hop]
                        if (topomap[ID-1][nID-1] != cost){
                            if (topomap[ID-1][nID-1] == -1 && cost != -1){
                                cout<<"now linked to node "<<nID<<" with cost "<<cost<<"\n";
                                IP_table[nID-1] = ipstr;
                            }
                            else if (topomap[ID-1][nID-1] != -1 && cost == -1){
                                cout<<"no longer linked to node "<<nID<<"\n";
                                IP_table[nID-1] = "";
                            }
                            else if (topomap[ID-1][nID-1] != -1 && cost != -1){
                                cout<<"now linked to node "<<nID<<" with cost "<<cost<<"\n";
                                IP_table[nID-1] = ipstr;
                            }
                            topomap[ID-1][nID-1] = cost;
                        }

                        topomap[nID-1][ID-1] = cost;
                        topomap[ID-1][nID-1] = cost;

                        string link = "";
                        for (int i=0; i<MAX_NODE_NUM; ++i){
                            if (topomap[ID-1][i] > 0){
                                link+= my_to_string(ID);
                                link+= " ";
                                link+= my_to_string(i+1); //nID
                                link+= " ";
                                link+= my_to_string(topomap[ID-1][i]); //cost
                                link+= " ";
                            }
                        }

                        flooded=1;
                        received_from[ID-1]=1;
                        for (int i=0; i<MAX_NODE_NUM; ++i){
                            if (topomap[ID-1][i] > 0){
                                struct sockaddr_in neighbors_addr;
                                bzero(&neighbors_addr,sizeof(neighbors_addr));
                                neighbors_addr.sin_family=AF_INET;
                                neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                neighbors_addr_len = sizeof neighbors_addr;
                                if ((numbytes=sendto(listenerfd, link.c_str(), strlen(link.c_str()), 
                                    0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                    perror("send error");
                                //cout<<"Sending "<<numbytes<<" bytes "<<link<<" to Node "<< (i+1)<<"\n";
                            }
                        }

                        
                        //cout<<"\n";
                    }                    
                }
               
            }
        }
    }
    close(listenerfd);
    close(sockfd);

    return 0;
}



