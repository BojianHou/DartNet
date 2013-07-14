// 文件名: stcp_server.c
//
// 描述: 这个文件包含服务器STCP套接字接口定义. 你需要实现所有这些接口.
//
// 创建日期: 2013年1月
//

#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"

//
//  用于服务器程序的STCP套接字API. 
//  ===================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void stcp_server_init(int conn)
{
	printf("----------------stcp server initial start!----------------\n");
	int i;
	for (i = 0; i < server_tcb_size; i++) {
		server_tcb[i] = NULL;
	}
	
	gSockFd = conn;
	
	pthread_t seghandle_thread;
	int rc;
	rc = pthread_create(&seghandle_thread, NULL, seghandler, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	printf("start the stcp server seghandler thread succesfully!\n");
	return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_sock(unsigned int server_port)
{
	printf("----------------stcp server sock start!----------------\n");
	int i;
  	for (i = 0; i < server_tcb_size; i++) {
  		if (server_tcb[i] == NULL) {
			printf("find a NULL tcb, initial it!\n");
  			server_tcb[i] = (server_tcb_t *)malloc(sizeof(struct server_tcb));
			server_tcb[i]->server_portNum = server_port;       //服务器端口号
			server_tcb[i]->client_portNum = 0;    //客户端端口号
			server_tcb[i]->state = CLOSED;     	//客户端状态
			server_tcb[i]->expect_seqNum = 1;       //新段准备使用的下一个序号 

			server_tcb[i]->server_nodeID = 0;     //服务器节点ID, 类似IP地址, 当前未使用
			server_tcb[i]->server_portNum = server_port;    //服务器端口号
			server_tcb[i]->client_nodeID = 0;     //客户端节点ID, 类似IP地址, 当前未使用
			server_tcb[i]->client_portNum = 0;    //客户端端口号
			server_tcb[i]->state = CLOSED;         	//服务器状态
			server_tcb[i]->expect_seqNum = 1;     //服务器期待的数据序号	
			server_tcb[i]->recvBuf = (char*)malloc(RECEIVE_BUF_SIZE);             //指向接收缓冲区的指针
			server_tcb[i]->usedBufLen = 0;       //接收缓冲区中已接收数据的大小
			server_tcb[i]->bufMutex = malloc(sizeof(pthread_mutex_t));      //指向一个互斥量的指针, 该互斥量用于对接收缓冲区的访问
			if (pthread_mutex_init(server_tcb[i]->bufMutex, PTHREAD_MUTEX_TIMED_NP) !=0 ){
				printf( "fail to creat buffer mutex.\n" );
				exit(-1);
			}
			break;
  		}
  	}
  	if (i == server_tcb_size) {
		printf("there is no NULL tcb, return -1\n");
  		return -1;
  	}
  	return i;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_accept(int sockfd)
{
	printf("----------------stcp server accept start!----------------\n");
	server_tcb[sockfd]->state = LISTENING;
	while (server_tcb[sockfd]->state != CONNECTED) {
		usleep(100000);//sleep for 100 milliseconds to receive the data from the server
	}
	printf("stcp server has accepted the client on %dth socket succesfully!\n", sockfd);
	return 1;
}

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_ROLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length)
{
	printf("----------------stcp server recv start!----------------\n");
	int i;
	while (1) {
		sleep(1);
		pthread_mutex_lock(server_tcb[sockfd]->bufMutex);
		printf("the length is %d\n", length);
		
		printf("the usedBufLen is: %d\n", server_tcb[sockfd]->usedBufLen);
		if (server_tcb[sockfd]->usedBufLen >= length) {
			memcpy(buf, server_tcb[sockfd]->recvBuf, length);
			char *p = server_tcb[sockfd]->recvBuf;
			p += length;
			memcpy(server_tcb[sockfd]->recvBuf, p, server_tcb[sockfd]->usedBufLen - length);
			server_tcb[sockfd]->usedBufLen -= length;
			pthread_mutex_unlock(server_tcb[sockfd]->bufMutex);
			return 1;
		}
		pthread_mutex_unlock(server_tcb[sockfd]->bufMutex);
	}
	return -1;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_close(int sockfd)
{
	while (server_tcb[sockfd]->usedBufLen != 0) {
		sleep(1);
	}
	printf("----------------stcp server close start!----------------\n");
	
	sleep(1);
	if (server_tcb[sockfd]->state != CLOSED) {
		free(server_tcb[sockfd]);
		pthread_mutex_destroy(server_tcb[sockfd]->bufMutex);
		free(server_tcb[sockfd]->recvBuf);
		server_tcb[sockfd] = NULL;
		printf("%dth sockfd has closed!\n", sockfd);
		return -1;
	}
	free(server_tcb[sockfd]);
	pthread_mutex_destroy(server_tcb[sockfd]->bufMutex);
	free(server_tcb[sockfd]->recvBuf);
	server_tcb[sockfd] = NULL;
	printf("%dth sockfd has closed!\n", sockfd);
	return 1;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* seghandler(void* arg)
{
	long i;
	int flag;
	unsigned int client_port;
	seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
	while (1) {
		flag = sip_recvseg(gSockFd, seg);
		if (flag == 1) {//报文丢失
			printf("the stcp server does'n receive a segment! segment is lost!\n");
			continue;
		}
		if (checkchecksum(seg) == -1) {
			printf("Checksum error!\n");
			continue;
		}
		if (flag == -1) {//接收不到报文，线程停止
			printf("can't receive anything in tcp level, the seghandler thread is going to end.\n");
			break;
		}
		for (i = 0; i < server_tcb_size; i++) {
			if (NULL != server_tcb[i] && (server_tcb[i]->server_portNum == seg->header.dest_port)) {
				break;
			}
		}
		if (i == server_tcb_size) {
			printf("the tcb you want to find does't exist!\n");
			continue;
		}
		client_port = seg->header.src_port;
		switch (server_tcb[i]->state) {
			case CLOSED:
				printf("stcp server now is in CLOSED state!\n");
				break;
			case LISTENING:
				printf("stcp server now is in LISTENING state!\n");
				if (seg->header.type == SYN) {
					printf("receive a SYN!\n");
					server_tcb[i]->state = CONNECTED;
					sendACK(i, client_port, SYNACK, seg);
				}
				break;
			case CONNECTED:
				printf("stcp server now is in CONNECTED state!\n");
				if (seg->header.type == SYN) {
					printf("receive a SYN!\n");
					server_tcb[i]->state = CONNECTED;
					server_tcb[i]->expect_seqNum = seg->header.seq_num;
					sendACK(i, client_port, SYNACK, seg);
				}
				else if (seg->header.type == FIN) {
					printf("receive a FIN!\n");
					server_tcb[i]->state = CLOSEWAIT;
					sendACK(i, client_port, FINACK, seg);
					pthread_t FINhandle_thread;
					int rc;
					rc = pthread_create(&FINhandle_thread, NULL, FINhandler, (void *)i);
					if (rc) {
						printf("ERROR; return code from pthread_create() is %d\n", rc);
						exit(-1);
					}
					
				}
				else if (seg->header.type == DATA) {
					printf("receive a DATA!\n");
					printf("the expect_seqNum is %d\n", server_tcb[i]->expect_seqNum);
					printf("the seqNum is %d\n", seg->header.seq_num);
					if (seg->header.seq_num == server_tcb[i]->expect_seqNum) {
						printf("the expect_seqNum == seq_num!\n");
						pthread_mutex_lock(server_tcb[i]->bufMutex);
						memcpy(server_tcb[i]->recvBuf + server_tcb[i]->usedBufLen, seg->data, seg->header.length);
						printf("the seg->header.length is %d\n", seg->header.length);
						server_tcb[i]->usedBufLen += seg->header.length;
						server_tcb[i]->expect_seqNum += seg->header.length;
						seg->header.src_port = server_tcb[i]->server_portNum;        //源端口号
						seg->header.dest_port = client_port;       //目的端口号
						seg->header.seq_num = 0;         //序号
						seg->header.ack_num = server_tcb[i]->expect_seqNum;         //确认号
						seg->header.length = 0;    //段数据长度
						seg->header.type = DATAACK;     //段类型
						seg->header.rcv_win = 0;  //当前未使用
						seg->header.checksum = 0;
						seg->header.checksum = checksum(seg);  //这个段的校验和
						sip_sendseg(gSockFd, seg);
						printf("stcp server send the changing DATAACK %d succesfully!\n", seg->header.ack_num);
						pthread_mutex_unlock(server_tcb[i]->bufMutex);
					}
					else {
						printf("the expect_seqNum != seq_num!\n");
						seg->header.src_port = server_tcb[i]->server_portNum;        //源端口号
						seg->header.dest_port = client_port;       //目的端口号
						seg->header.seq_num = 0;         //序号
						seg->header.ack_num = server_tcb[i]->expect_seqNum;         //确认号
						seg->header.length = 0;    //段数据长度
						seg->header.type = DATAACK;     //段类型
						seg->header.rcv_win = 0;  //当前未使用
						seg->header.checksum = 0;
						seg->header.checksum = checksum(seg);  //这个段的校验和
						sip_sendseg(gSockFd, seg);
						printf("stcp server send the not changed DATAACK %d succesfully!\n", seg->header.ack_num);
					}
				}
				break;
			case CLOSEWAIT:
				printf("stcp server now is in CLOSEWAIT state!\n");
				if (seg->header.type == FIN) {
					printf("receive a FIN!\n");
					sendACK(i, client_port, FINACK, seg);
				}
				break;
		}
	}
  	return 0;
}

void *FINhandler(void* index) {
	usleep(100000);//sleep for 100 ms 
	long i = (long)index;
	server_tcb[i]->state = CLOSED;
}

void sendACK(int index, unsigned int client_port, int type, seg_t* seg) {
	printf("the type stcp server sends is %d\n", type);

	seg->header.src_port = server_tcb[index]->server_portNum;        //源端口号
	seg->header.dest_port = client_port;       //目的端口号
	seg->header.seq_num = 0;         //序号
	seg->header.ack_num = server_tcb[index]->expect_seqNum;         //确认号
	seg->header.length = 0;    //段数据长度
	seg->header.type = type;     //段类型
	seg->header.rcv_win = 0;  //当前未使用
	seg->header.checksum = 0;
	seg->header.checksum = checksum(seg);  //这个段的校验和

	server_tcb[index]->expect_seqNum++;
	sip_sendseg(gSockFd, seg);
	printf("stcp server send the %d ACK succesfully!\n", type);
}

