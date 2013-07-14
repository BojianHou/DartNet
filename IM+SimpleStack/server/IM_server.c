#include "../common/head.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "../stcp/stcp_client.h"
#include "../stcp/stcp_server.h"
#include "../common/seg.h"

#define CLIENTPORT1 87
#define SERVERPORT1 88

//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {
	int sockfd;
	struct sockaddr_in servaddr;
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("SIP: Problem in creating the socket\n");
		return -1;
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(SIP_PORT);

	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("SIP: Problem in connecting to the server\n");
		return -1;
	}
	return sockfd;
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {
	close(sip_conn);	
}

void *HandleClient(void *threadindex) {
	struct packet_IM sendpkt, recvpkt;
	int iteration;
	int index = *((int*)threadindex);
	int sockfd = sockfdlist[index];
	int sockfd2 = sockfdlist2[index];
	while (1) {
		if (stcp_server_recv(sockfd, &recvpkt, sizeof(recvpkt)) == -1) {
			//error: client terminated prematurely
			printf("%s terminated prematurely\n", recvpkt.srcname);
			free(friendlist[index]);
			friendlist[index] = NULL;
			sendpkt.service = LOGOFF;
			strcpy(sendpkt.srcname, recvpkt.srcname); 
			for (iteration = 0; iteration < NUM_THREADS; iteration++) {
				if (friendlist[iteration] != NULL) {
					stcp_client_send(sockfdlist2[iteration], &sendpkt, sizeof(sendpkt));
				}
			}
			sockfdlist[index] = 0;
			pthread_exit(NULL);
		}
		if (recvpkt.service == LOGIN) {
			//printf("the received service is LOGIN\n");
			for (iteration = 0; iteration < NUM_THREADS; iteration++) {
				if (friendlist[iteration] != NULL) {
					if (strcmp(recvpkt.srcname, friendlist[iteration]) == 0) {
						sendpkt.status = REFUSED;//the account which client creates has already existed
						break;
					}
				}
			}
			if (iteration == NUM_THREADS) {//the account which client creates doesn't exist
				strcpy(sendpkt.srcname, recvpkt.srcname);
				sendpkt.service = INFORM; 
				for (iteration = 0; iteration < NUM_THREADS; iteration++) {//to inform other friends that I have loged in
					if (friendlist[iteration] != NULL) {
						stcp_client_send(sockfdlist2[iteration], &sendpkt, sizeof(sendpkt));
					}
				}
				friendlist[index] = (char *) malloc(sizeof(char));
				strcpy(friendlist[index], recvpkt.srcname);
				sendpkt.status = LOGED;
			}
			stcp_client_send(sockfd2, &sendpkt, sizeof(sendpkt));
			//printf("has sent pkt %d back\n", sendpkt.status);
			memset(&sendpkt, 0, sizeof(sendpkt));
		}
		else if (recvpkt.service == LOGOFF) {
			free(friendlist[index]);
			friendlist[index] = NULL;
			printf("%s are offline!\n", recvpkt.srcname);
			sendpkt.service = LOGOFF;
			strcpy(sendpkt.srcname, recvpkt.srcname); 
			for (iteration = 0; iteration < NUM_THREADS; iteration++) {
				if (friendlist[iteration] != NULL) {
					stcp_client_send(sockfdlist2[iteration], &sendpkt, sizeof(sendpkt));
				}
			}
			sockfdlist[index] = INFINITE;
			sockfdlist2[index] = INFINITE;
			stcp_server_close(sockfd);
			stcp_client_disconnect(sockfd2);
			stcp_client_close(sockfd2);
			pthread_exit(NULL);
			//close(sockfd);
			//close(sockfd2);
		}
		else if (recvpkt.service == MESSAGE) {
			for (iteration = 0; iteration < NUM_THREADS; iteration++) {//to find if the desname exists
				if (friendlist[iteration] != NULL) {
					//printf("the desname is %s\n\n", recvpkt.desname);
					if (strcmp(recvpkt.desname, friendlist[iteration]) == 0)
						break;
				}
			}
			if (iteration == NUM_THREADS) {//the desname dose not exist
				printf("the desname does not exist\n");
				sendpkt.status = NOTEXIST;
				sendpkt.service = MESSAGE;
				stcp_client_send(sockfd2, &sendpkt, sizeof(sendpkt));
			}
			else {//the desname exist
				stcp_client_send(sockfdlist2[iteration], &recvpkt, sizeof(recvpkt));
				printf("have sent message %s to %s\n", recvpkt.message, recvpkt.desname);
				//send(sockfdlist[iteration], (void *)&recvpkt, sizeof(recvpkt), 0);
			}
			memset(&sendpkt, 0, sizeof(sendpkt));
			
		}
		else if (recvpkt.service == ONLINEFRI) {
			for (iteration = 0; iteration < NUM_THREADS; iteration++) {
				if (friendlist[iteration] != NULL) {
					strcpy(sendpkt.message + iteration * NAMESIZE, friendlist[iteration]);
				}
			}
			sendpkt.service = ONLINEFRI;
			stcp_client_send(sockfd2, &sendpkt, sizeof(sendpkt));
			//send(sockfd, (void *)&sendpkt, sizeof(sendpkt), 0);
			memset((void *)&sendpkt, 0, sizeof(sendpkt));
			memset((void *)&recvpkt, 0, sizeof(recvpkt));
		}
		else if (recvpkt.service == SENDTOALL) {
			recvpkt.service = MESSAGE;
			for (iteration = 0; iteration < NUM_THREADS; iteration++) {
				if (friendlist[iteration] != NULL) {
					stcp_client_send(sockfdlist2[iteration], &recvpkt, sizeof(recvpkt));
					//send(sockfdlist[iteration], (void *)&recvpkt, sizeof(recvpkt), 0);
				}
			}
		}
	}
	
}




int main(int argc, char **argv)
{	
	int index;
	int rc;
	pthread_t threads[NUM_THREADS];

/*******************************wait for connecting to clients**********************************************/
	//连接到SIP进程并获得TCP套接字描述符
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//初始化STCP服务器
	stcp_server_init(sip_conn);

	//启动seghandler线程
	pthread_t seghandle_thread;
	rc = pthread_create(&seghandle_thread, NULL, seghandler, &sip_conn);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
/********************************************************************************/
	
	for (index = 0; index < NUM_THREADS; index++) {
		friendlist[index] = NULL;
	}
	for (index = 0; index < NUM_THREADS; index++) {
		sockfdlist[index] = INFINITE;
	}
	while(1) {
		/*=================================================================*/
		//在端口SERVERPORT1上创建STCP服务器套接字 
		int sockfd = stcp_server_sock(SERVERPORT1);
		//printf("the sockfd is %d\n", sockfd);
		if(sockfd<0) {
			printf("can't create stcp server\n");
			exit(1);
		}
		//监听并接受来自STCP客户端的连接 
		printf("receiving other client...........................\n");
		stcp_server_accept(sockfd);
		printf("Received request...\n");

		/*char hostname[50];
		printf("Enter server name to connect:");
		scanf("%s",hostname);
		int server_nodeID = topology_getNodeIDfromname(hostname);
		if(server_nodeID == -1) {
			printf("host name error!\n");
			exit(1);
		} else {
			printf("connecting to node %d\n",server_nodeID);
		}*/
		int server_nodeID;
		stcp_server_recv(sockfd, &server_nodeID, sizeof(int));

		//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88
		int sockfd2 = stcp_client_sock(CLIENTPORT1);
		//printf("the sockfd2 is %d\n", sockfd2);
		if(sockfd2<0) {
			printf("fail to create stcp client sock");
			exit(1);
		}
		if(stcp_client_connect(sockfd2,server_nodeID,SERVERPORT1)<0) {
			printf("fail to connect to stcp server\n");
			exit(1);
		}
		/*=================================================================*/
		
		for (index = 0; index < NUM_THREADS; index++) {
			if (sockfdlist[index] == INFINITE) {
				sockfdlist[index] = sockfd;//用来收包
				sockfdlist2[index] = sockfd2;//用来发包
				break;
			}
		}
		
		
/***************************************create thread for every client**************************************/
		rc = pthread_create(&threads[index], NULL, HandleClient, &index);
		printf("Creating thread %d\n", index);
		//close(connfd);
	}
}


