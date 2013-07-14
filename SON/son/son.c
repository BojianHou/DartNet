//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) {
	//你需要编写这里的代码.
	//Create a socket for the socket
	//If sockfd<0 there was an error in the creation of the socket
	
	int i, j;
	int listenfd, connfd;
	int client_num = 0;
	int myID = topology_getMyNodeID();
	int nbr_num = topology_getNbrNum();
	for (i = 0; i < nbr_num; i++) {
		if (myID < nt[i].nodeID) {
			client_num++;
		}
	}
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;

	if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Problem in creating the socket");
		exit(2);
	}
	
	//preparation of the socket address
	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(CONNECTION_PORT);
	
	//bind the socket
	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	
	//listen to the socket by creating a connection queue, then wait for clients
	listen(listenfd, client_num);
	printf("Server running...waiting for connections.\n");

	for (i = 0; i < client_num; i++) {
		clilen = sizeof(cliaddr);
		//accept a connection
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
		for (j = 0; j < nbr_num; j++) {
			if (nt[j].nodeID == topology_getNodeIDfromip(&cliaddr.sin_addr)) {
				nt[j].conn = connfd;
				break;
			}
		}
		printf("Received request...\n");
	}
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	printf("I am in connectNbrs!\n");
	//你需要编写这里的代码.
	//Create a socket for the client
	//If sockfd<0 there was an error in the creation of the socket
	int i;
	int server_num = 0;
	int myID = topology_getMyNodeID();
	int nbr_num = topology_getNbrNum();
	for (i = 0; i < nbr_num; i++) {
		if (myID > nt[i].nodeID) {
			server_num++;
		}
	}
	for (i = 0; i < nbr_num; i++) {
		if (myID > nt[i].nodeID) {
			struct sockaddr_in servaddr;
			int sockfd;
			if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) <0) {
				perror("Problem in creating the socket");
				exit(2);
			}
			//Creation of the socket
			memset(&servaddr, 0, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_addr.s_addr = htonl(nt[i].nodeIP);////////字节序要注意转换——Hobo
			//printf("the nodeIP is %x\n", htonl(nt[i].nodeIP));
			servaddr.sin_port = htons(CONNECTION_PORT); //convert to big-endian order

			//Connection of the client to the socket
			if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
				perror("Problem in connnecting to the server");
				exit(3);
			}
			//printf("client sockfd: %d\n", sockfd);
			nt[i].conn = sockfd;
			//printf("the client conn is %d and the ID is %d---------------------haha\n", nt[i].conn, nt[i].nodeID);
			//system("clear");
			printf("Connect to the server successfully!\n");
		}
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	//你需要编写这里的代码.
	int* index = (int*)arg;
	sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
	while (1) {
		recvpkt(pkt, nt[*index].conn);//接收从邻居发来的数据
		sleep(10);
		forwardpktToSIP(pkt, sip_conn);//转发给本地SIP
	}
	return 0;
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	//你需要编写这里的代码.
	//Create a socket for the socket
	//If sockfd<0 there was an error in the creation of the socket
	
	int i;
	int nbr_num = topology_getNbrNum();
	int listenfd, connfd;
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;

	if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Problem in creating the socket");
		exit(2);
	}
	
	//preparation of the socket address
	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SON_PORT);
	
	//bind the socket
	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	
	//listen to the socket by creating a connection queue, then wait for clients
	listen(listenfd, 1);
	printf("Server running...waiting for connections.\n");

	clilen = sizeof(cliaddr);
	//accept a connection
	connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
	sip_conn = connfd;
	printf("Received request...\n");

	sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(struct packet));
	int* nextNode = (int*)malloc(sizeof(int));
	while (1) {
		getpktToSend(pkt, nextNode, connfd);
		if ((*nextNode) == BROADCAST_NODEID) {
			for (i = 0; i < nbr_num; i++) {
				sendpkt(pkt, nt[i].conn);
			}
		}
		else {
			for (i = 0; i < nbr_num; i++) {
				if (*nextNode == nt[i].nodeID)
					break;
			}
			sendpkt(pkt, nt[i].conn);
		}
	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	//你需要编写这里的代码.
	int i;
	for (i = 0; i < topology_getNbrNum(); i++) {
		close(nt[i].conn);
	}
	close(sip_conn);
	free(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	printf("have waited for 60s\n");
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	
	printf("the waitNbrs has returned--------Hobo\n");

	//此时, 所有与邻居之间的连接都建立好了
	for (i = 0; i < nbrNum; i++) {
		printf("the conn %d is: %d\n", i, nt[i].conn);
	}
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
