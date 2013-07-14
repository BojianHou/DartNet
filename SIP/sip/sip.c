//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2013年1月

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 30

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() { 
	//你需要编写这里的代码.
	//Create a socket for the client
	//If sockfd<0 there was an error in the creation of the socket
	printf("we are now in connectToSON!\n");
	struct sockaddr_in servaddr;
	int sockfd;
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) <0) {
		perror("Problem in creating the socket");
		exit(2);
	}

	//Creation of the socket
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//connect to the server
	
	servaddr.sin_port = htons(SON_PORT); //convert to big-endian order
	//Connection of the client to the socket
	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
		perror("Problem in connnecting to the server");
		exit(3);
	}
	system("clear");
	printf("Connect to the server successfully!\n");
	printf("we are now off connectToSON!\n");
	return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	//你需要编写这里的代码.
	printf("we are now in routeupdate_daemon\n");
	sip_pkt_t *pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t)); 
	int entryNum = topology_getNodeNum();
	pkt->header.src_nodeID = topology_getMyNodeID();
	pkt->header.dest_nodeID = BROADCAST_NODEID;
	pkt->header.length = entryNum * sizeof(routeupdate_entry_t) + 4;
	pkt->header.type = ROUTE_UPDATE;
	pkt_routeupdate_t* routeUp = (pkt_routeupdate_t*)pkt->data;
	
	routeUp->entryNum = entryNum;
	//memcpy(routeUp->entry, dv[0].dvEntry, sizeof(dv_t));
	
	while (1) {
		pthread_mutex_lock(dv_mutex);
		memcpy(routeUp->entry, dv[0].dvEntry, sizeof(dv_entry_t) * entryNum);
		son_sendpkt(BROADCAST_NODEID, pkt, son_conn);
		printf("has broadcast to the son\n");
		/*printf("---the sent src_nodeID is %d\n", pkt->header.src_nodeID);
		printf("---the sent dest_nodeID is %d\n", pkt->header.dest_nodeID);
		printf("---the sent length is %d\n", pkt->header.length);
		printf("---the sent type is ROUTE_UPDATE\n");*/
		pthread_mutex_unlock(dv_mutex);
		sleep(ROUTEUPDATE_INTERVAL);
	}
	printf("we are now off routeupdate_daemon\n");
	pthread_exit(NULL);
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	//你需要编写这里的代码.
	printf("we are now in pkthandler in sip!\n");
	sip_pkt_t pkt;
	int slot_num;
	routingtable_entry_t *ptr;
	int myID = topology_getMyNodeID();
	while(son_recvpkt(&pkt,son_conn) > 0) {
		if (pkt.header.type == SIP) {
			printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
			//printf("---the received dest_nodeID is %d\n", pkt.header.dest_nodeID);
			//printf("---the received length is %d\n", pkt.header.length);
			//printf("---the received type is SIP\n");
			if (pkt.header.dest_nodeID == myID) {
				forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, (seg_t*)pkt.data);
				switch(((seg_t*)(pkt.data))->header.type) {
					case 0:
						printf("SIP forward the seg --SYN-- to the STCP!\n");
						break;
					case 1:
						printf("SIP forward the seg --SYNACK-- to the STCP!\n");
						break;
					case 2:
						printf("SIP forward the seg --FIN-- to the STCP!\n");
						break;
					case 3:
						printf("SIP forward the seg --FINACK-- to the STCP!\n");
						break;
					case 4:
						//printf("SIP forward the seg --DATA-- to the STCP!\n");
						break;
					case 5:
						//printf("SIP forward the seg --DAYAACK-- to the STCP!\n");
						break;
				}
			}
			else {
				slot_num = makehash(pkt.header.dest_nodeID);
				ptr = routingtable->hash[slot_num];
				while (ptr != NULL) {//寻找是否存在给定目的节点的路由条目
					if (pkt.header.dest_nodeID == ptr->destNodeID)
						break;
					ptr = ptr->next;
				}
				if (ptr != NULL) {//根据路由表转发报文给下一跳
					son_sendpkt(ptr->nextNodeID, &pkt, son_conn);
					printf("sip forward the seg to the nextnode%d!\n", ptr->nextNodeID);
				}
			}
		}
		else if (pkt.header.type == ROUTE_UPDATE) {
			/*================本实验精华所在================*/
			//printf("---the received type is ROUTE_UPDATE\n");
			pkt_routeupdate_t *routeUp = (pkt_routeupdate_t *)pkt.data;
			pthread_mutex_lock(dv_mutex);
			pthread_mutex_lock(routingtable_mutex);
			int i, j, srcNode = pkt.header.src_nodeID;
			int entryNum = topology_getNodeNum(), nbrNum = topology_getNbrNum();
			for(j = 0;j < entryNum;j ++)
				dvtable_setcost(dv, srcNode,routeUp->entry[j].nodeID, routeUp->entry[j].cost);
			for(i = 0;i < entryNum;i ++) {
				if(topology_getMyNodeID() == (dv + i)->nodeID)
					break;
			}
			dv_entry_t *myEntry = (dv + i)->dvEntry;
			int *nbr = topology_getNbrArray();
			for(i = 0;i < entryNum;i ++) {
				srcNode = topology_getMyNodeID();
				int destNode = (myEntry + i)->nodeID;
				for(j = 0;j < nbrNum;j ++)	{
					int midNode = *(nbr + j);
					int firstCost = nbrcosttable_getcost(nct, midNode);
					int lastCost = dvtable_getcost(dv, midNode, destNode);
					if(firstCost + lastCost < dvtable_getcost(dv, srcNode, destNode)) {
						dvtable_setcost(dv, srcNode, destNode, firstCost + lastCost);
						routingtable_setnextnode(routingtable, destNode, midNode);
					}
				}
			}
			pthread_mutex_unlock(routingtable_mutex);
			pthread_mutex_unlock(dv_mutex);
		}
		else {
			printf("error type\n");
			//exit(0);
		}
	}
	close(son_conn);
	son_conn = -1;
	printf("we are now off pkthandler!\n");
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	//你需要编写这里的代码.
	close(son_conn);
	close(stcp_conn);
	nbrcosttable_destroy(nct);
	dvtable_destroy(dv);
	routingtable_destroy(routingtable);	
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.--------------------------->这个要好好思考



void waitSTCP() {
	//你需要编写这里的代码.
	//Create a socket for the socket
	//If sockfd<0 there was an error in the creation of the socket
	printf("we are now in waitSTCP!\n");
	int slot_num;
	sip_pkt_t* pkt;
	int myID = topology_getMyNodeID();
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
	servaddr.sin_port = htons(SIP_PORT);
	
	//bind the socket
	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	
	//listen to the socket by creating a connection queue, then wait for clients
	listen(listenfd, MAX_NODE_NUM);
	printf("Server running...waiting for connections.\n");

	//accept a connection
	while(1) {
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
		stcp_conn = connfd;
		printf("Received request...\n");

		seg_t* segPtr = (seg_t*)malloc(sizeof(seg_t));
		int* dest_nodeID = (int*)malloc(sizeof(int));
		while (getsegToSend(stcp_conn, dest_nodeID, segPtr) > 0) {
			switch(segPtr->header.type) {
				case 0:
					printf("SIP get some seg --SYN-- from STCP to send%d!\n", stcp_conn);
					break;
				case 1:
					printf("SIP get some seg --SYNACK-- from STCP to send%d!\n", stcp_conn);
					break;
				case 2:
					printf("SIP get some seg --FIN-- from STCP to send%d!\n", stcp_conn);
					break;
				case 3:
					printf("SIP get some seg --FINACK-- from STCP to send%d!\n", stcp_conn);
					break;
				case 4:
					//printf("SIP get some seg --DATA-- from STCP to send!\n");
					break;
				case 5:
					//printf("SIP get some seg --DAYAACK-- from STCP to send!\n");
					break;
			}
			slot_num = makehash(*dest_nodeID);
			routingtable_entry_t* ptr = routingtable->hash[slot_num];
			while (ptr != NULL) {//寻找是否存在给定目的节点的路由条目
				if (*dest_nodeID == ptr->destNodeID)
					break;
				ptr = ptr->next;
			}
			if (ptr != NULL) {//根据路由表转发报文给下一跳
				pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
				pkt->header.src_nodeID = myID;
				pkt->header.dest_nodeID = *dest_nodeID;
				pkt->header.length = 24 + segPtr->header.length;
				pkt->header.type = SIP;//包含在报文中的数据是一个STCP段（包括段首部和数据）——Hobo
				memcpy(pkt->data, segPtr, pkt->header.length);
				son_sendpkt(ptr->nextNodeID, pkt, son_conn);
				printf("sip has send pkt to the son %d %d\n", ptr->nextNodeID, son_conn);
			}
			else {
				printf("can not get the destnodeID!\n");
				//exit(0);
			}
		}
	}
	printf("we are now off waitSTCP!\n");
	return;
}


int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();//邻居代价表
	dv = dvtable_create();//距离矢量表
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));//距离矢量表互斥变量
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();//路由表
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));//路由表互斥变量
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}




