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
//在连接到SIP进程后, 等待1秒, 让服务器启动
#define STARTDELAY 1

struct packet_IM sendpkt, recvpkt;
struct sockaddr_in servaddr;

char srcname[100];
char desname[100];
char order[100];
char message[MAXLINE];

int service;
int sip_conn;
int sockfd, sockfd2;


//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {
	int sockfd;
	struct sockaddr_in servaddr;
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("STCP: Problem in creating the socket\n");
		return -1;
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(SIP_PORT);

	if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		perror("STCP: Problem in connecting to the server\n");
		return -1;
	}
	system("clear");
	printf("Connect to the server successfully!\n");
	return sockfd;
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {
	close(sip_conn);
}

void *recvmessage() {
	int index;
	while (1) {
		//printf("111111111\n");
		if (stcp_server_recv(sockfd2, &recvpkt, sizeof(recvpkt)) == -1) {
			//error: server terminated prematurely
			perror("The server terminated prematurely");
			exit(4);
		}
		//printf("22222222\n");
		if (recvpkt.service == MESSAGE) {
			if (recvpkt.status == NOTEXIST) {
				printf("The friend you want to chat with does not exist.\n");
			}
			else {
				printf("%s: %s\n", recvpkt.srcname, recvpkt.message);
			}
			
		}
		else if (recvpkt.service == ONLINEFRI) {
			printf("The online friends are as follow:\n");
			for (index = 0; index < NUM_THREADS; index++) {
				if (*(recvpkt.message + index * NAMESIZE) != '\0') {
					printf("%s\n", recvpkt.message + index * NAMESIZE);
				}
			}
		}
		else if (recvpkt.service == INFORM) {
			printf("%s has loged in.\n", recvpkt.srcname);
		}
		else if (recvpkt.service == LOGOFF) {
			printf("%s has loged off.\n", recvpkt.srcname);
		}
		//printf("555555\n");
	}
}

int main(int argc, char **argv) {
/***************************************connect to the server***********************************************/
	//连接到SIP进程并获得TCP套接字描述符	
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("fail to connect to the local SIP process\n");
		exit(1);
	}

	//初始化stcp客户端
	stcp_client_init(sip_conn);

	//启动seghandler线程
	pthread_t seghandle_thread;
	int rc;
	rc = pthread_create(&seghandle_thread, NULL, seghandler, &sip_conn);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}

	printf("start the stcp client seghandler thread succesfully!\n");
	sleep(STARTDELAY);

	char hostname[50];
	printf("Enter server name to connect:");
	scanf("%s",hostname);
	int server_nodeID = topology_getNodeIDfromname(hostname);
	if(server_nodeID == -1) {
		printf("host name error!\n");
		exit(1);
	} else {
		printf("connecting to node %d\n",server_nodeID);
	}

	//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88
	sockfd = stcp_client_sock(CLIENTPORT1);
	//printf("the sockfd is %d\n", sockfd);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,server_nodeID,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	int myID = topology_getMyNodeID();
	stcp_client_send(sockfd, &myID, sizeof(int));
	//printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//在端口SERVERPORT2上创建另一个STCP服务器套接字
	sockfd2= stcp_server_sock(SERVERPORT1);
	//printf("the sockfd2 is %d\n", sockfd2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd2);
/************************************************log in*****************************************************/
	while (1) {
		printf("Welcome to HappyChat!\nPlease input your username(without space) to log in:");
		/* 清空缓冲区 */
		int ch;
		while ((ch = getchar()) != '\n' && ch != EOF);
		//scanf(" %[^\n]", srcname);
		gets(srcname);
		sendpkt.service = LOGIN;
		strcpy(sendpkt.srcname, srcname);

		stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
		if (stcp_server_recv(sockfd2, &recvpkt, sizeof(recvpkt)) == -1) {
			//error: server terminated prematurely
			perror("The server terminated prematurely");
			exit(4);
		}
		while (recvpkt.status == REFUSED) {
			/* 清空缓冲区 */
			//while ((ch = getchar()) != '\n' && ch != EOF);
			printf("The user has already existed! Please retype another username:");
			gets(srcname);
			strcpy(sendpkt.srcname, srcname);
			stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
			if (stcp_server_recv(sockfd2, &recvpkt, sizeof(recvpkt)) == -1) {
				//error: server terminated prematurely
				perror("The server terminated prematurely");
				exit(4);
			}
		}
		while (recvpkt.status == BUSY) {
			/* 清空缓冲区 */
			//while ((ch = getchar()) != '\n' && ch != EOF);
			printf("The server is very busy, please type your username again:\n");
			gets(srcname);
			strcpy(sendpkt.srcname, srcname);
			stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
			if (stcp_server_recv(sockfd2, &recvpkt, sizeof(recvpkt)) == -1) {
				//error: server terminated prematurely
				perror("The server terminated prematurely");
				exit(4);
			}
		}
		if (recvpkt.status == LOGED) {
			printf("You are online now, enjoy your time!\n");
			printf("Please follow the rules as follow:\nType in SENDTO NAME MESSAGE if you want to send message to some particular friend online\nType in CHECK if you want to know all the friends online\nType in ALL MESSAGE if you want to send message to all the friends online\nType in LOGOFF if you want to exit\n");
			break;
		}
		//fflush(stdout);
	}
/**************************************create receive thread to receive message*****************************/
	pthread_t recvthread;
	rc = pthread_create(&recvthread, NULL, recvmessage, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	
/***************************************log in**************************************************************/
	while (1) {
		/* 清空缓冲区 */
		//int ch;
		//while ((ch = getchar()) != '\n' && ch != EOF);
		scanf("%s", order);
		if (strcmp(order, "SENDTO") == 0) {
			scanf("%s", desname);
			//while ((ch = getchar()) != '\n' && ch != EOF);
			gets(message);
			strcpy(sendpkt.srcname, srcname);
			strcpy(sendpkt.desname, desname);
			strcpy(sendpkt.message, message);
			sendpkt.service = MESSAGE;
			stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
		}
		else if (strcmp(order, "CHECK") == 0) {
			sendpkt.service = ONLINEFRI;
			stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
		}
		else if (strcmp(order, "ALL") == 0) {
			/* 清空缓冲区 */
			//while ((ch = getchar()) != '\n' && ch != EOF);
			gets(message);
			//printf("the message is: %s\n", message);
			sendpkt.service = SENDTOALL;
			strcpy(sendpkt.srcname, srcname);
			strcpy(sendpkt.message, message);
			stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
		}
		else if (strcmp(order, "LOGOFF") == 0) {
			sendpkt.service = LOGOFF;
			strcpy(sendpkt.srcname, srcname);
			stcp_client_send(sockfd, &sendpkt, sizeof(sendpkt));
			stcp_client_disconnect(sockfd);
			stcp_client_close(sockfd);
			sleep(1);
			printf("%s is going off line...\n", srcname);
			stcp_server_close(sockfd2);
			//关掉与SIP的连接
			disconnectToSIP(sip_conn);
			break;
		}
		else {
			printf("Invalid input!\n");
		}
		//fflush(stdout);
		memset((void *)&sendpkt, 0, sizeof(sendpkt));
		memset((void *)&recvpkt, 0, sizeof(recvpkt));
	}
	//printf("off main!!!\n");
	//exit(0);
}











