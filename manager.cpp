/*
** server.c -- a stream socket server demo
to do:
	1. random input
	2. win or lose
	3. fix algorithm for score
	4. cosmetic mbellishments
	5. Error handling for user input (if theres time)
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
#include <signal.h>
#include <string>
#include <iostream>
#include <sstream>
#include <time.h>       /* time */
#include <fstream>
#include <pthread.h>
#include <vector>

#define PORT "3490"  // the port users will be connecting to
#define MAXDATASIZE 100
#define MAXATTEMPTS 8
#define BACKLOG 10	 // how many pending connections queue will hold
#define MAX_NODE_NUM 16
#define MAX_MSG_NUM 20
#define MAX_MSG_SIZE 200

using namespace std;

struct thread_data_t{
	int thread_vid;
	int new_fd;
};

struct msg{
    int src;
    int dst;
    string text;
};

pthread_t threads[MAX_NODE_NUM];
pthread_mutex_t	vidLock;
pthread_attr_t attr;
string ipstr[MAX_NODE_NUM];
int currVID = 0; // the current lowest VID
thread_data_t threadData[MAX_NODE_NUM];
int topology[MAX_NODE_NUM][MAX_NODE_NUM]; // the graph topology
msg msgs[MAX_MSG_NUM];
int convergece = 0;
fd_set master,read_fds;    // master file descriptor list
int fdmax;        // maximum file descriptor number
int node_alive = 0;
int num_of_msg = 0;
int initial_process = 1;
int initial_node_num = 0;
//pthread_mutex_t mut;

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
		ipstr[i] = "";
	}
}

int parseMsgs(msg msgs[], const char* filename){
    FILE *fp=fopen(filename,"r");

    if(!fp){
        printf("failure: can't open message file!\n");
        return 0;
    }
    
    for(int k=0; k<MAX_MSG_NUM; k++){
        msgs[k].src = 0;
        msgs[k].dst = 0;
        msgs[k].text = "";
    }


    int k = 0;
    while(!feof(fp)){
        char tmp[MAX_MSG_SIZE], ch;
        memset(tmp, 0, MAX_MSG_SIZE);
        fscanf(fp,"%d %d ", &msgs[k].src, &msgs[k].dst);
        int i = 0;
        while((ch=fgetc(fp))!='\n' && !feof(fp)){
            tmp[i]=ch;
            i++;
        }
        i++;
        tmp[i]='\n';

        string bufstr2(tmp); 
        msgs[k].text = bufstr2;
        k++;
        //cout<<"F "<<msgs[k].src<<" "<<msgs[k].dst<<" "<<msgs[k].text<<"\n";
    }

    fclose(fp);
    return k;
}

int parseTopology(int topology[][MAX_NODE_NUM], const char* filename){
    FILE *fp=fopen(filename,"r");

    if(!fp){
        printf("failure: can't open topology file!\n");
        return 0;
    }
    //Read Tolology

    int n1, n2, n_max = 0, cost;
    while(!feof(fp)){
        fscanf(fp,"%d %d %d\n",&n1,&n2,&cost);
        topology[n1-1][n2-1]=cost;
        topology[n2-1][n1-1]=cost;
        if (n_max < n1) n_max = n1;
        if (n_max < n2) n_max = n2;
    }
    initial_node_num = n_max;
    fclose(fp);
    return 1;
}

void *tcpThread(void *threadarg)
{
	long tid;
	int fd_new;

	struct thread_data_t *my_data;
   	my_data = (struct thread_data_t *) threadarg;
   	tid = my_data->thread_vid;
   	fd_new = my_data->new_fd;

   	pthread_mutex_lock (&vidLock);
 	currVID++;
    pthread_mutex_unlock (&vidLock);

    //Sends the ID of the new node
    string nodeID;
	nodeID = my_to_string(tid+1);
	string neighbors = nodeID;
	string self = "";
	neighbors += " ";

    for(int i =0 ;i < MAX_NODE_NUM;i++){
    	self = my_to_string(i+1);
		self += " ";
    	if(topology[tid][i] > 0 && ipstr[i] != "" && tid != i){
    		neighbors+= my_to_string(i+1);
    		neighbors+= " ";
    		neighbors+= my_to_string(topology[tid][i]);
    		neighbors+= " ";
    		neighbors+= ipstr[i];
    		neighbors+= " ";
    		self+= my_to_string(tid+1);
    		self+=" ";
    		self+= my_to_string(topology[tid][i]);
    		self+=" ";
    		self+= ipstr[i];
    		self+=" ";

			//printf("Sending: %s \n", neighbors.c_str() );
			//printf("Sending: %s \n", self.c_str() );
			if (send(threadData[i].new_fd, self.c_str(), strlen(self.c_str()), 0) == -1)
				perror("send error");
    	}
    }

    if (send(fd_new, neighbors.c_str(), strlen(neighbors.c_str()), 0) == -1)
		perror("send error");

    char buf[256];

    while (1){
		int numbytes = recv(fd_new, buf, 256, 0);
		
		if (numbytes < 0) {
			perror("recv");
			exit(1);
		}
		
		buf[numbytes] = '\0';
		pthread_mutex_lock (&vidLock); 
		//cout<<"numbytes "<<numbytes<<"\n";
		++convergece;
		//cout<<convergece<<"\n";
		//cout<<"node_alive "<<node_alive<<"\n";
		if(convergece >= node_alive){
			//cout<<"All Nodes Converged!\n";
			//cout<<"initial_node_num "<<initial_node_num<<" node_alive "<<node_alive<<"\n";
			convergece = 0;
			//cout<<"do send3\n";
			//cout<<node_alive<<" "<<initial_node_num<<" "<<initial_process<<"\n";
			if(node_alive == initial_node_num || initial_process == 0){
				initial_process = 0;
				//cout<<"do send1\n";

				for (int i=0; i<node_alive; ++i){
				//cout<<"do send2\n";
					if (send(threadData[i].new_fd, "cvg", 4, 0) == -1)
						perror("send error");	
				}
			}
			if (initial_process == 0){
				sleep(1);
				//cout<<"Sending Message\n";
				for (int i=0; i<num_of_msg; ++i){
					if (msgs[i].src <= currVID && msgs[i].src != 0){
						//cout<<"What\n";
						string msgtosend = "from ";
						msgtosend+= my_to_string(msgs[i].src);
						msgtosend+= " to ";
						msgtosend+= my_to_string(msgs[i].dst);
						msgtosend+= " hops ";
						msgtosend+= my_to_string(msgs[i].src);
						msgtosend+= " message ";
						msgtosend+= msgs[i].text;
						if (send(threadData[msgs[i].src-1].new_fd, msgtosend.c_str(), strlen(msgtosend.c_str()), 0) == -1){
							perror("send msg error");
							cout<<"Sending error "<<msgtosend<<"\n";
						}
						//cout<<"Sending "<<msgtosend<<"\n";
					}
				}	
			}
		}
		pthread_mutex_unlock (&vidLock);
		//cout<<"conv "<<convergece<<"\n";
		
		// if(convergece >= node_alive && (node_alive == initial_node_num || initial_process == 0)){
		// 	initial_process = 0;
		// 	for (int i=0; i<node_alive; ++i){
		// 		if (send(threadData[i].new_fd, "cvg", 4, 0) == -1)
		// 			perror("send error");	
		// 	}
		// }
		
		// if(convergece >= node_alive && initial_process == 0){
		// 	convergece = 0;
		// 	cout<<"All Nodes Converged!\n";
		// 	for (int i=0; i<num_of_msg; ++i){
		// 		if (msgs[i].src <= currVID){
		// 			string msgtosend = "from ";
		// 			msgtosend+= my_to_string(msgs[i].src);
		// 			msgtosend+= " to ";
		// 			msgtosend+= my_to_string(msgs[i].dst);
		// 			msgtosend+= " hops ";
		// 			msgtosend+= my_to_string(msgs[i].src);
		// 			msgtosend+= " message ";
		// 			msgtosend+= msgs[i].text;
		// 			if (send(threadData[msgs[i].src-1].new_fd, msgtosend.c_str(), strlen(msgtosend.c_str()), 0) == -1)
		// 				perror("send error");	
		// 		}
		// 	}
		// }

    }
    //sendEndMessage(fd_new);
    
    close(fd_new);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char buf[256];
	string neighbors = "", self = "";

   	pthread_mutex_init(&vidLock, NULL);
   	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

    if (argc != 3) {
        fprintf(stderr,"[usage] [topology filename] [message filename]\n");
        exit(1);
    }

	initTopology(topology);
	if(!parseTopology(topology, argv[1]))
		return -1;

	num_of_msg = parseMsgs(msgs, argv[2]);
	// for (int k=0; k<MAX_NODE_NUM; ++k){
	// 	cout<<msgs[k].src<<" "<<msgs[k].dst<<" "<<msgs[k].text<<"\n";
	// }


	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	//printf("\nwaiting for connections...\n");

	//Select accept() and STDIN
	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);    // clear the master and temp sets
    FD_SET(STDIN_FILENO,&master); /* add STDIN to connset */
    FD_SET(sockfd, &master); /* add listenerfd to connset */

    // keep track of the biggest file descriptor
    fdmax = sockfd; // so far, it's this one

	while(1) {  // main accept() loop
		//printf("Input new topology:\n");
		read_fds = master;
		struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if(select(fdmax+1,&read_fds,NULL,NULL,NULL)<0){
            fprintf(stdout, "select() error\n");
            exit(4);
        } 

        for(int i = 0; i <= fdmax; i++) {
        	
            if (FD_ISSET(i, &read_fds)) { // we got one!!
            	//cout<<i<<'\n';
                if (i == STDIN_FILENO) {
                	int node1, node2, cost;
                	int node1alive_before = 0, node2alive_before = 0;
                	int node1alive_after = 0, node2alive_after = 0;
                	scanf("%d %d %d", &node1, &node2, &cost);
					//printf("Node 1: %d, Node 2: %d, Cost: %d\n", node1, node2, cost);
                    // for (int i=0; i < MAX_NODE_NUM; ++i){
                    // 	if (topology[node1][i] > 0){
                    // 		node1alive_before = 1;
                    // 		break;
                    // 	} 
                    // 	if (topology[node2][i] > 0){
                    // 		node2alive_before = 1;
                    // 		break;
                    // 	} 
                    // }

                    topology[node1-1][node2-1] = cost;
                    topology[node2-1][node1-1] = cost;

                    // for (int i=0; i < MAX_NODE_NUM; ++i){
                    // 	if (topology[node1][i] > 0){
                    // 		node1alive_after = 1;
                    // 		break;
                    // 	} 
                    // 	if (topology[node2][i] > 0){
                    // 		node2alive_after = 1;
                    // 		break;
                    // 	} 
                    // }

                    if (ipstr[node1-1] != "" && ipstr[node2-1] != ""){
                		neighbors = my_to_string(node1);
			    		neighbors += " ";
			    		neighbors += my_to_string(node2);
			    		neighbors += " ";
			    		neighbors += my_to_string(cost);
			    		neighbors += " ";
						neighbors += ipstr[i];
						neighbors += " ";
						self = my_to_string(node2);
			    		self += " ";
			    		self += my_to_string(node1);
			    		self += " ";
			    		self += my_to_string(cost);
			    		self += " ";
						self += ipstr[i];
						self += " ";		    		

						//printf("Sending: %s \n", neighbors.c_str() );
						//printf("Sending: %s \n", self.c_str() );

						if (send(threadData[node1-1].new_fd, neighbors.c_str(), strlen(neighbors.c_str()), 0) == -1)
							perror("send error");
						if (send(threadData[node2-1].new_fd, self.c_str(), strlen(self.c_str()), 0) == -1)
							perror("send error");

						for (int i=0; i<node_alive; ++i){
							if (send(threadData[i].new_fd, "recvg", 6, 0) == -1)
								perror("send error");
						}
                    }
        		}

        		else if (i == sockfd){
					sin_size = sizeof their_addr;
					new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

					if (new_fd == -1) {
                        perror("accept");
                        continue;
                    } 
					inet_ntop(their_addr.ss_family,
						get_in_addr((struct sockaddr *)&their_addr),
						s, sizeof s);
					//printf("server: got connection from %s\n", s);
					++node_alive;
					ipstr[currVID] = s;
					threadData[currVID].thread_vid = currVID;
					threadData[currVID].new_fd = new_fd;
					pthread_create(&threads[currVID], &attr, tcpThread, (void *) &threadData[currVID]);
				}
			}
		}


	}


	return 0;
}

