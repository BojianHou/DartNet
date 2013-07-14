//文件名: server/app_simple_server.c

//描述: 这是简单版本的服务器程序代码. 服务器首先连接到本地SIP进程. 然后它调用stcp_server_init()初始化STCP服务器. 
//它通过两次调用stcp_server_sock()和stcp_server_accept()创建2个套接字并等待来自客户端的连接. 服务器然后接收来自两个连接的客户端发送的短字符串. 
//最后, 服务器通过调用stcp_server_close()关闭套接字, 并断开与本地SIP进程的连接.

//创建日期: 2013年1月

//输入: 无

//输出: STCP服务器状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <netdb.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "../stcp/stcp_client.h"
#include "../stcp/stcp_server.h"


//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//在接收到字符串后, 等待15秒, 然后关闭连接.
#define WAITTIME 15

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

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//初始化STCP服务器
	stcp_server_init(sip_conn);

	//启动seghandler线程
	pthread_t seghandle_thread;
	int rc;
	rc = pthread_create(&seghandle_thread, NULL, seghandler, &sip_conn);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}

	//在端口SERVERPORT1上创建STCP服务器套接字 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

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
	int sockfd2 = stcp_client_sock(CLIENTPORT1);
	if(sockfd2<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd2,server_nodeID,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}

	char buf1[6];

	//接收来自第一个连接的字符串
	int i;
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//通过第二个连接发送字符串
    char mydata2[7] = "byebye";
	for(i=0;i<5;i++){
      	stcp_client_send(sockfd2, mydata2, 7);
		printf("send string:%s to connection 2\n",mydata2);	
    }

	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}
	sleep(WAITTIME);
	//关闭连接
	if(stcp_client_disconnect(sockfd2)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	
	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}
