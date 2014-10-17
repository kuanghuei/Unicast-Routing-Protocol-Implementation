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

struct route {
    int nexthop;
    int cost;
};

route rt[MAX_NODE_NUM];
int D_table[MAX_NODE_NUM][MAX_NODE_NUM];
string IP_table[MAX_NODE_NUM];

void initRoutingTable(){

    for(int i=0;i<MAX_NODE_NUM; i++){
        rt[i].nexthop = 0;
        rt[i].cost = -1;
        //IP_table[i] = "";
    }
}

void initDtable(){

    for(int i=0;i<MAX_NODE_NUM; i++){
        for(int j=0;j<MAX_NODE_NUM; j++){
            D_table[i][j] = -1;
            //[Destination][Next Hop]
        }
    }
}

int main(int argc, char *argv[]){

    int ID, nID;
    if (argc != 2) {
        fprintf(stderr,"usage: client-hostname server-port\n");
        exit(1);
    }
    for(int i=0;i<MAX_NODE_NUM; i++){
        IP_table[i] = "";
    }

    initRoutingTable();
    initDtable();
    int sockfd, listenerfd, numbytes;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buf[256];
    int neighbors_addr_len;
    //string bufstr;
    string neighbors = "", self = "";

    if (argc != 2) {
        fprintf(stderr,"[usage] hostname\n");
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
    rt[ID-1].nexthop = ID;
    rt[ID-1].cost = 0;
    bufv.erase(bufv.begin());

    while(!bufv.empty()){
        nID = stoi(bufv[0]);
        D_table[nID-1][nID-1] = stoi(bufv[1]);
        rt[nID-1].nexthop = nID;
        rt[nID-1].cost = stoi(bufv[1]);
        IP_table[nID-1] = bufv[2];
        //cout<<rt[nID-1].id<<" "<<rt[nID-1].nexthop<<" "<<rt[nID-1].cost<<" "<<rt[nID-1].ipstr<<'\n';
        cout<<"now linked to node "<<nID<<" with cost "<<stoi(bufv[1])<<"\n";
        bufv.erase(bufv.begin(), bufv.begin()+3);
    }
    
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

    //send out all topology to neighbors
    //From_ID Cost_1 Cost_2 .... 
    string routing_vector = my_to_string(ID);

    for (int i=0; i<MAX_NODE_NUM; ++i){
        routing_vector+= " ";
        routing_vector+= my_to_string(rt[i].cost);    
    }

    for (int i=0; i<MAX_NODE_NUM; ++i){
        if (rt[i].cost > 0 && i != ID-1){
            struct sockaddr_in neighbors_addr;
            bzero(&neighbors_addr,sizeof(neighbors_addr));
            neighbors_addr.sin_family=AF_INET;
            neighbors_addr.sin_port=htons(PORTNUM+i+1);
            neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
            neighbors_addr_len = sizeof neighbors_addr;

            if (sendto(listenerfd, routing_vector.c_str(), strlen(routing_vector.c_str()), 
                0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len) == -1)
                perror("send error");
            //cout<<"Sending "<<routing_vector<<" to Node "<< (i+1)<<"\n";
        }
    }

    fd_set master, read_fds;    // master file descriptor list
    FD_ZERO(&master);    // clear the master and temp sets
    FD_SET(sockfd,&master); /* add STDIN to connset */
    FD_SET(listenerfd, &master); /* add listenerfd to connset */
    int fdmax = listenerfd;
    int convergence = 0;
    struct timeval tv;
    
    int convergetime = 1;
    tv.tv_sec = convergetime;
    tv.tv_usec = 0;
    int defaulttimeout = 200000;
    int cleared = 0;

    while(1) {  // main accept() loop
        //printf("Complete\n");
        read_fds = master;
        int selectret;
        if((selectret = select(fdmax+1,&read_fds,NULL,NULL,&tv)) < 0){
            fprintf(stdout, "select() error\n");
            exit(4);
        }
        //cout<<selectret<<convergence<<convergetime<<"\n";
        if (selectret == 0 && convergence == 0 && tv.tv_sec == convergetime){
            //cout<<"check\n";
            // for (int i=0; i<MAX_NODE_NUM; ++i){
            //     if (rt[i].cost != -1)
            //         cout<<i+1<<" "<<rt[i].nexthop<<" "<<rt[i].cost<<"\n";
            // }
            
            //cout<<"Converged!\n";
            cleared = 0;
            convergence = 1;
            tv.tv_sec = defaulttimeout;
            if (send(sockfd, "c", 2, 0) == -1)
                perror("send error");
        }

        //cout<<"still waiting...";
        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listenerfd){
                    
                    //maybe squeezed into one packet
                    //memset(buf, 0, sizeof(buf));
                    struct sockaddr_in node_addr;
                    socklen_t node_len;
                    int numbytes;
                    memset(buf, 0, sizeof(buf));
                    if ((numbytes = recvfrom(listenerfd, buf, 256, 0, (struct sockaddr*)&node_addr, &node_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }
                    //parse
                    string bufstr2(buf); 
                    vector<string> bufv2 = split(bufstr2, ' ');
                    //cout<<"Get "<<numbytes<<" from neighbor "<<bufstr2<<"\n";
                    
                    if (bufv2[0] == "clear"){
                        //cout<<"clear\n";
                        if (cleared == 0){
                            cleared = 1;
                            for (int i=0; i<MAX_NODE_NUM; i++){
                                for (int j=0; j<MAX_NODE_NUM; j++){
                                    if (i != j){
                                        D_table[i][j] = -1;
                                    }
                                }
                            }
                            D_table[ID-1][ID-1] = 0; 
                            initRoutingTable();
                            rt[ID-1].nexthop = ID;
                            rt[ID-1].cost = 0;
                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                for (int j=0; j<MAX_NODE_NUM; ++j){
                                    if (D_table[i][j] != -1){
                                        rt[i].cost = D_table[i][j];
                                        rt[i].nexthop = j+1;
                                    }
                                }
                            }
                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                if (IP_table[i] != ""){
                                    struct sockaddr_in neighbors_addr;
                                    bzero(&neighbors_addr,sizeof(neighbors_addr));
                                    neighbors_addr.sin_family=AF_INET;
                                    neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                    neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                    neighbors_addr_len = sizeof neighbors_addr;
                                    if ((numbytes=sendto(listenerfd, "clear", 6, 
                                        0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                        perror("send error");
                                }
                            }
                            // for (int i=0; i<MAX_NODE_NUM; ++i){
                            //     for (int j=0; j<MAX_NODE_NUM; ++j){
                            //         cout<<D_table[i][j]<<" ";
                            //     }
                            //     cout<<"\n";
                            // }
                            D_table[ID-1][ID-1] = 0; 
                            rt[ID-1].nexthop = ID;
                            rt[ID-1].cost = 0;
                        }
                    }
                    //if message
                    else if (bufv2[0] == "from"){
                        cout<<bufstr2<<"\n";
                        if (stoi(bufv2[3]) == ID);
                        else if (rt[stoi(bufv2[3])-1].cost == -1){
                            cout<<"There is no route to "<<bufv2[3]<<"\n";
                        }
                        else {
                            int nexthop = rt[stoi(bufv2[3])-1].nexthop;
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
                        convergence = 0;
                        tv.tv_sec = convergetime;
                    //split horrizon D_table[destination][next hop]
                        nID = stoi(bufv2[0]);
                        int change = 0;
                        //cout<<"ID"<<ID<<" nID"<<nID<<"\n";
                        for (int i=0; i<MAX_NODE_NUM; ++i){
                            if (i != nID-1 && i != ID-1){
                                if (stoi(bufv2[i+1]) != -1){
                                    if ((D_table[i][nID-1] == -1 || 
                                        D_table[i][nID-1] != D_table[nID-1][nID-1] + stoi(bufv2[i+1]))
                                            && stoi(bufv2[i+1]) > 0){
                                        //cout<<"change\n";
                                        D_table[i][nID-1] = D_table[nID-1][nID-1] + stoi(bufv2[i+1]);
                                        //update rt
                                        int cost_pre = rt[i].cost;

                                        int smallest = 1000;
                                        int nexthop;
                                        for (int j=0; j<MAX_NODE_NUM; ++j){
                                            if (D_table[i][j] < smallest && D_table[i][j] != -1){
                                                smallest = D_table[i][j];
                                                nexthop = j+1;
                                            }
                                        }
                                        if (smallest == 1000){
                                            smallest = -1;
                                            nexthop = 0;
                                        } 
                                        if (cost_pre != smallest){
                                            rt[i].cost = smallest;
                                            rt[i].nexthop = nexthop;
                                            
                                        }
                                        change = 1;  
                                    }
                                } else {
                                    if (D_table[i][nID-1] != -1){
                                        D_table[i][nID-1] = -1;
                                        //update rt
                                        int cost_pre = rt[i].cost;

                                        int smallest = 1000;
                                        int nexthop;
                                        for (int j=0; j<MAX_NODE_NUM; ++j){
                                            if (D_table[i][j] < smallest && D_table[i][j] != -1){
                                                smallest = D_table[i][j];
                                                nexthop = j+1;
                                            }
                                        }
                                        if (smallest == 1000){
                                            smallest = -1;
                                            nexthop = 0;
                                        } 
                                        if (cost_pre != smallest){
                                            rt[i].cost = smallest;
                                            rt[i].nexthop = nexthop;
                                            
                                        }
                                        change = 1;  
                                    }
                                }
                            }
                        }
                    
                        //cout<<"Changed? "<<change<<"\n";
                        //for (int i=0; i<MAX_NODE_NUM; ++i){
                        //    cout<<rt[i].cost<<"["<<rt[i].nexthop<<"]";
                        //}
                        //cout<<"\n";
                        if (change){
                            //send to neighbor
                            routing_vector = my_to_string(ID);

                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                routing_vector+= " ";
                                routing_vector+= my_to_string(rt[i].cost);    
                            }
                            routing_vector+= " ";

                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                if (rt[i].cost > 0 && i != ID-1 && rt[i].nexthop == i+1){
                                    struct sockaddr_in neighbors_addr;
                                    bzero(&neighbors_addr,sizeof(neighbors_addr));
                                    neighbors_addr.sin_family=AF_INET;
                                    neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                    neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                    neighbors_addr_len = sizeof neighbors_addr;
                                    if ((numbytes=sendto(listenerfd, routing_vector.c_str(), strlen(routing_vector.c_str()), 
                                        0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                        perror("send error");
                                    //cout<<"Resending "<<numbytes<<" bytes "<<routing_vector<<" to Node "<< (i+1)<<"\n";
                                }
                            }
                        }
                    }
                }
                else if (i == sockfd) {
                    
                    //recv from manager
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
                    //cout<<bufstr2<<"\n";
                    //if message from manager
                    if (bufv2[0] == "from"){
                        cout<<bufstr2<<"\n";
                        if (stoi(bufv2[3]) == ID); //reach destination
                        else if (rt[stoi(bufv2[3])-1].cost == -1){
                            cout<<"There is no route to "<<bufv2[3]<<"\n";
                        }
                        else {
                            int nexthop = rt[stoi(bufv2[3])-1].nexthop;
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
                                 perror("sendto error");
                            //cout<<msgtosend<<"\n";
                        }
                    } else if (bufv2[0] == "cvg") {
                        //cout<<bufv2[0]<<"\n";
                        for (int i=0; i<MAX_NODE_NUM; ++i){
                            if (rt[i].cost != -1)
                                cout<<i+1<<" "<<rt[i].nexthop<<" "<<rt[i].cost<<"\n";
                        }
                    } else if (bufv2[0] == "recvg"){
                        convergence = 0;
                        tv.tv_sec = convergetime;
                    }
                    //topology change
                    else {
                        convergence = 0;
                        tv.tv_sec = convergetime;
                        //cout<<"Get "<<numbytes<<" from manager "<<bufstr2<<"\n";
                        nID = stoi(bufv2[1]);
                        int cost = stoi(bufv2[2]);
                        string ipstr = bufv2[3];
                        //update D table
                        //[destination][next hop]
                        if (D_table[nID-1][nID-1] == cost)
                            continue;

                        if (cost >= 0 && D_table[nID-1][nID-1] >= 0){
                            cleared = 1;
                            for (int i=0; i<MAX_NODE_NUM; i++){
                                for (int j=0; j<MAX_NODE_NUM; j++){
                                    if (i != j){
                                        D_table[i][j] = -1;
                                    }
                                }
                            }
                            initRoutingTable();
                            rt[ID-1].nexthop = ID;
                            rt[ID-1].cost = 0;
                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                if (IP_table[i] != ""){
                                    struct sockaddr_in neighbors_addr;
                                    bzero(&neighbors_addr,sizeof(neighbors_addr));
                                    neighbors_addr.sin_family=AF_INET;
                                    neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                    neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                    neighbors_addr_len = sizeof neighbors_addr;
                                    if ((numbytes=sendto(listenerfd, "clear", 6, 
                                        0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                        perror("send error");
                                }
                            }
                            D_table[ID-1][ID-1] = 0;
                            D_table[nID-1][nID-1] = cost;
                            // cout<<"o1 diff\n";
                            // for (int i=0; i<MAX_NODE_NUM; ++i){
                            //     for (int j=0; j<MAX_NODE_NUM; ++j){
                            //         cout<<D_table[i][j]<<" ";
                            //     }
                            //     cout<<"\n";
                            // }

                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                //cout<<"check\n";
                                for (int j=0; j<MAX_NODE_NUM; ++j){
                                    if (D_table[i][j] != -1){
                                        rt[i].cost = D_table[i][j];
                                        rt[i].nexthop = j+1;
                                    }
                                }
                            }
                            cout<<"now linked to node "<<nID<<" with cost "<<cost<<"\n";
                            //IP_table[nID-1] = ipstr;
                        }
                        else if (cost >= 0 && D_table[nID-1][nID-1] == -1){
                            //cout<<"o2\n";
                            D_table[nID-1][nID-1] = cost;
                            rt[nID-1].cost = cost;
                            rt[nID-1].nexthop = nID;
                            IP_table[nID-1] = ipstr;
                            cout<<"now linked to node "<<nID<<" with cost "<<cost<<"\n";
                        }
                        else {
                            cleared = 1;
                            for (int i=0; i<MAX_NODE_NUM; i++){
                                for (int j=0; j<MAX_NODE_NUM; j++){
                                    if (i != j){
                                        D_table[i][j] = -1;
                                    }
                                }
                            }
                            D_table[ID-1][ID-1] = 0; 
                            initRoutingTable();
                            rt[ID-1].nexthop = ID;
                            rt[ID-1].cost = 0;
                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                if (IP_table[i] != ""){
                                    struct sockaddr_in neighbors_addr;
                                    bzero(&neighbors_addr,sizeof(neighbors_addr));
                                    neighbors_addr.sin_family=AF_INET;
                                    neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                    neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                    neighbors_addr_len = sizeof neighbors_addr;
                                    if ((numbytes=sendto(listenerfd, "clear", 6, 
                                        0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                        perror("send error");
                                }
                            }
                            D_table[ID-1][ID-1] = 0;
                            D_table[nID-1][nID-1] = -1;
                            // for (int i=0; i<MAX_NODE_NUM; ++i){
                            //     for (int j=0; j<MAX_NODE_NUM; ++j){
                            //         cout<<D_table[i][j]<<" ";
                            //     }
                            //     cout<<"\n";
                            // }

                            for (int i=0; i<MAX_NODE_NUM; ++i){
                                //cout<<"check\n";
                                for (int j=0; j<MAX_NODE_NUM; ++j){
                                    if (D_table[i][j] != -1){
                                        rt[i].cost = D_table[i][j];
                                        rt[i].nexthop = j+1;
                                    }
                                }
                            }
                            D_table[ID-1][ID-1] = 0;
                            rt[ID-1].nexthop = ID;
                            rt[ID-1].cost = 0;
                            cout<<"no longer linked to node "<<nID<<"\n";
                            IP_table[nID-1] = "";
                        }
                        //send to neighbor
                        routing_vector = my_to_string(ID);

                        for (int i=0; i<MAX_NODE_NUM; ++i){
                            routing_vector+= " ";
                            routing_vector+= my_to_string(rt[i].cost);    
                        }
                        routing_vector+= " ";

                        for (int i=0; i<MAX_NODE_NUM; ++i){
                            if (rt[i].cost > 0 && i != ID-1 && rt[i].nexthop == i+1){
                                struct sockaddr_in neighbors_addr;
                                bzero(&neighbors_addr,sizeof(neighbors_addr));
                                neighbors_addr.sin_family=AF_INET;
                                neighbors_addr.sin_port=htons(PORTNUM+i+1);
                                neighbors_addr.sin_addr.s_addr = inet_addr(IP_table[i].c_str()); 
                                neighbors_addr_len = sizeof neighbors_addr;
                                if ((numbytes=sendto(listenerfd, routing_vector.c_str(), strlen(routing_vector.c_str()), 
                                    0, (struct sockaddr*) &neighbors_addr, neighbors_addr_len)) == -1)
                                    perror("send error");
                                //cout<<"IP: "<<IP_table[i].c_str()<<" port: "<<PORTNUM+i+1<<"\n";
                                //cout<<"Sending "<<numbytes<<" bytes "<<routing_vector<<" to Node "<< (i+1)<<"\n";
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
