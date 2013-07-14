//文件名: client/stcp_client.c
//
//描述: 这个文件包含STCP客户端接口实现 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"

//声明tcbtable为全局变量
client_tcb_t* client_tcb[MAX_TRANSPORT_CONNECTIONS];
//声明到SIP进程的TCP连接为全局变量
int sip_conn;

/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
void stcp_client_init(int conn) 
{
    printf("----------------stcp client initial start!----------------\n");
	int i;
	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		client_tcb[i] = NULL;
	}
	
	sip_conn = conn;
	
	pthread_t seghandle_thread;
	int rc;
	rc = pthread_create(&seghandle_thread, NULL, seghandler, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	printf("start the stcp client seghandler thread succesfully!\n");
	
	return;
}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_client_sock(unsigned int client_port) 
{
	printf("----------------stcp client sock start!----------------\n");
  	int i;
  	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
  		if (client_tcb[i] == NULL) {
			printf("find a NULL tcb, initial it!\n");
  			client_tcb[i] = (client_tcb_t *)malloc(sizeof(struct client_tcb));
			client_tcb[i]->server_nodeID = 0;        //服务器节点ID, 类似IP地址
			client_tcb[i]->server_portNum = 0;       //服务器端口号
			client_tcb[i]->client_nodeID = 0;     //客户端节点ID, 类似IP地址
			client_tcb[i]->client_portNum = client_port;    //客户端端口号
			client_tcb[i]->state = CLOSED;     	//客户端状态
			client_tcb[i]->next_seqNum = 1;       //新段准备使用的下一个序号 
			client_tcb[i]->bufMutex = malloc(sizeof(pthread_mutex_t));      //发送缓冲区互斥量
			client_tcb[i]->sendBufHead = NULL;          //发送缓冲区头
			client_tcb[i]->sendBufunSent = NULL;        //发送缓冲区中的第一个未发送段
			client_tcb[i]->sendBufTail = NULL;          //发送缓冲区尾
			client_tcb[i]->unAck_segNum = 0;      //已发送但未收到确认段的数量
			if (pthread_mutex_init(client_tcb[i]->bufMutex, PTHREAD_MUTEX_TIMED_NP) !=0 ){
				printf( "fail to creat buffer mutex.\n" );
				exit(-1);
			}
			break;
  		}
  	}
  	if (i == MAX_TRANSPORT_CONNECTIONS) {
		printf("there is no NULL tcb, return -1\n");
  		return -1;
  	}
  	return i;
}

// 这个函数用于连接服务器. 它以套接字ID, 服务器节点ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器节点ID和服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	printf("----------------stcp client connect start!----------------\n");
  	int tryno;
  	tryno = 0;
  	client_tcb[sockfd]->server_portNum = server_port;
	client_tcb[sockfd]->state = SYNSENT;
	client_tcb[sockfd]->server_nodeID = nodeID;
	
	seg_t *seg = (seg_t*)malloc(sizeof(seg_t));

	seg->header.src_port = client_tcb[sockfd]->client_portNum;        //源端口号
	seg->header.dest_port = server_port;       //目的端口号
	seg->header.seq_num = client_tcb[sockfd]->next_seqNum;         //序号
	seg->header.ack_num = 0;         //确认号
	seg->header.length = 0;    //段数据长度
	seg->header.type = SYN;     //段类型
	seg->header.rcv_win = 0;  //当前未使用
	seg->header.checksum = 0;
	seg->header.checksum = checksum(seg);  //这个段的校验和
	client_tcb[sockfd]->next_seqNum++;
  	sip_sendseg(sip_conn, nodeID, seg);
	usleep(SYN_TIMEOUT / 1000);
  	while (client_tcb[sockfd]->state != CONNECTED) {
		printf("the time to connect to the stcp server is out, start resend!\n");
		sip_sendseg(sip_conn, nodeID, seg);
		if (tryno > SYN_MAX_RETRY) {
			printf("the resend time is over SYN_MAX_RETRY, the state turn to CLOSED and return -1!\n");
			client_tcb[sockfd]->state = CLOSED;
			return -1;
		}
		tryno++;
		usleep(SYN_TIMEOUT / 1000);
  	}
	printf("connect to the stcp server on %dth socket succesfully! and the state now is: %d\n", sockfd, client_tcb[sockfd]->state);
  	return 1;
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目.
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生. 
// 这个函数在成功时返回1，否则返回-1. 
// stcp_client_send是一个非阻塞函数调用.
// 因为用户数据被分片为固定大小的STCP段, 所以一次stcp_client_send调用可能会产生多个segBuf
// 被添加到发送缓冲区链表中. 如果调用成功, 数据就被放入TCB发送缓冲区链表中, 根据滑动窗口的情况,
// 数据可能被传输到网络中, 或在队列中等待传输.

int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
    pthread_mutex_lock(client_tcb[sockfd]->bufMutex);
	while (length > 0) {
		segBuf_t *segbuf = (segBuf_t*)malloc(sizeof(segBuf_t));
		
		segbuf->seg.header.src_port = client_tcb[sockfd]->client_portNum;        //源端口号
		segbuf->seg.header.dest_port = client_tcb[sockfd]->server_portNum;       //目的端口号
		segbuf->seg.header.seq_num = client_tcb[sockfd]->next_seqNum;         //序号
		segbuf->seg.header.ack_num = 0;         //确认号
		segbuf->seg.header.type = DATA;     //段类型
		segbuf->seg.header.rcv_win = 0;  //当前未使用

		if (length > MAX_SEG_LEN) {
			segbuf->seg.header.length = MAX_SEG_LEN;    //段数据长度
			memcpy(segbuf->seg.data, data, MAX_SEG_LEN);
			data = (char*)data;
			data += MAX_SEG_LEN;
			length -= MAX_SEG_LEN;
		}
		else {
			segbuf->seg.header.length = length;    //段数据长度
			memcpy(segbuf->seg.data, data, length);
			length -= length;
		}
		
		segbuf->seg.header.checksum = 0;
		segbuf->seg.header.checksum = checksum(&(segbuf->seg));  //这个段的校验和

		segbuf->sentTime = getCurrentTime();
		segbuf->next = NULL;
		client_tcb[sockfd]->next_seqNum += segbuf->seg.header.length;
		
		

		if (client_tcb[sockfd]->sendBufHead == NULL) {
			//printf("the sendBufHead is NULL!\n");
			pthread_t sendBuf_timer_thread;
			int rc;
			rc = pthread_create(&sendBuf_timer_thread, NULL, sendBuf_timer, (void*)client_tcb[sockfd]);
			if (rc) {
				printf("ERROR; return code from pthread_create() is %d\n", rc);
				exit(-1);
			}
			printf("start the stcp client sendbuf_timer thread succesfully!\n");

			/* 插入到缓冲区中 */
			client_tcb[sockfd]->sendBufHead = segbuf;
			client_tcb[sockfd]->sendBufunSent = segbuf;
			client_tcb[sockfd]->sendBufTail = segbuf;
			//发送
			sip_sendseg(sip_conn, client_tcb[sockfd]->server_nodeID, &(segbuf->seg));
			printf("the segbuf->seg.header.length is %d and the seqno is %d\n", segbuf->seg.header.length, segbuf->seg.header.seq_num);
			client_tcb[sockfd]->sendBufunSent = client_tcb[sockfd]->sendBufunSent->next;
			client_tcb[sockfd]->unAck_segNum++;
		}
		else {
			//插入到缓冲区尾部
			client_tcb[sockfd]->sendBufTail->next = segbuf;
			client_tcb[sockfd]->sendBufTail = segbuf;
			if (client_tcb[sockfd]->sendBufunSent == NULL) {
				printf("the sendBufunSent is NULL!\n");
				client_tcb[sockfd]->sendBufunSent = segbuf;
				if (client_tcb[sockfd]->unAck_segNum < GBN_WINDOW) {
					sip_sendseg(sip_conn, client_tcb[sockfd]->server_nodeID, &(segbuf->seg));
					printf("the segbuf->seg.header.length is %d and the seqno is %d\n", segbuf->seg.header.length, segbuf->seg.header.seq_num);
					client_tcb[sockfd]->sendBufunSent = client_tcb[sockfd]->sendBufunSent->next;
					client_tcb[sockfd]->unAck_segNum++;
				}
			}
			else {//sendBufunSent不为null，则从sendBufunSent开始发送后续段
				while (client_tcb[sockfd]->unAck_segNum < GBN_WINDOW && client_tcb[sockfd]->sendBufunSent != NULL) {
					printf("sent the unsent buf %d!\n", client_tcb[sockfd]->sendBufunSent->seg.header.seq_num);
					printf("the segbuf->seg.header.length is %d and the seqno is %d\n", segbuf->seg.header.length, segbuf->seg.header.seq_num);
					sip_sendseg(sip_conn, client_tcb[sockfd]->server_nodeID, &(client_tcb[sockfd]->sendBufunSent->seg));
					client_tcb[sockfd]->sendBufunSent = client_tcb[sockfd]->sendBufunSent->next;
					client_tcb[sockfd]->unAck_segNum++;
				}
			}
		}
		
		
	}
	pthread_mutex_unlock(client_tcb[sockfd]->bufMutex);
	return 0;
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.

int stcp_client_disconnect(int sockfd) 
{
	while(client_tcb[sockfd]->sendBufHead != NULL) {
		usleep(100000);
	}
	printf("----------------stcp client disconnect start!----------------\n");
	
  	int tryno;
  	tryno = 0;
	
	seg_t *seg = (seg_t*)malloc(sizeof(seg_t));

	seg->header.src_port = client_tcb[sockfd]->client_portNum;        //源端口号
	seg->header.dest_port = client_tcb[sockfd]->server_portNum;       //目的端口号
	seg->header.seq_num = client_tcb[sockfd]->next_seqNum;         //序号
	seg->header.ack_num = 0;         //确认号
	seg->header.length = 0;    //段数据长度
	seg->header.type = FIN;     //段类型
	seg->header.rcv_win = 0;  //当前未使用
	seg->header.checksum = 0;
	seg->header.checksum = checksum(seg);  //这个段的校验和

	client_tcb[sockfd]->next_seqNum++;
  	sip_sendseg(sip_conn, client_tcb[sockfd]->server_nodeID, seg);
	printf("client has sent a FIN!%d %d\n", sip_conn, client_tcb[sockfd]->server_nodeID);
	client_tcb[sockfd]->state = FINWAIT;
	
	usleep(FIN_TIMEOUT / 1000);
	
  	while (client_tcb[sockfd]->state == FINWAIT) {
		printf("the time to disconnect from the stcp server is out, start resend!\n");
		sip_sendseg(sip_conn, client_tcb[sockfd]->server_nodeID, seg);
		printf("client has sent a FIN!%d %d\n", sip_conn, client_tcb[sockfd]->server_nodeID);
		tryno++;
		if (tryno > FIN_MAX_RETRY) {
			printf("the resend time is over FIN_MAX_RETRY, the state turn to CLOSED and return -1!\n");
			client_tcb[sockfd]->state = CLOSED;
			segBuf_t *p = client_tcb[sockfd]->sendBufHead;
			while (p != NULL) {//清空缓冲区
				client_tcb[sockfd]->sendBufHead = p->next;
				free(p);
				p = client_tcb[sockfd]->sendBufHead;
				client_tcb[sockfd]->unAck_segNum--;
			}
			return -1;
		}
		usleep(FIN_TIMEOUT / 1000);
  	}
	segBuf_t *p = client_tcb[sockfd]->sendBufHead;
	while (p != NULL) {//清空缓冲区
		client_tcb[sockfd]->sendBufHead = p->next;
		free(p);
		p = client_tcb[sockfd]->sendBufHead;
		client_tcb[sockfd]->unAck_segNum--;
	}
  	return 1;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_client_close(int sockfd) 
{
	printf("----------------stcp client close start!----------------\n");
	if (client_tcb[sockfd]->state != CLOSED) {
		free(client_tcb[sockfd]);
		pthread_mutex_destroy(client_tcb[sockfd]->bufMutex);
		client_tcb[sockfd] = NULL;
		printf("%dth socket has closed\n", sockfd);
		return -1;
	}
	free(client_tcb[sockfd]);
	pthread_mutex_destroy(client_tcb[sockfd]->bufMutex);
	client_tcb[sockfd] = NULL;
	printf("%dth socket has closed\n", sockfd);
	return 1;
}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	printf("we are now in seghandler!\n");
	int i;
	int flag;
	while (1) {
		seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
		int* src_nodeID = (int*)malloc(sizeof(int));
		flag = sip_recvseg(sip_conn, src_nodeID, seg);
		if (flag == 1) {//报文丢失
			printf("the stcp client does'n receive a segment! segment is lost!\n");
			continue;
		}
		if (flag == -1) {//接收不到报文，线程停止
			printf("can't receive anything in tcp level, the seghandler thread is going to end.\n");
			break;
		}
		if (checkchecksum(seg) == -1) {
			printf("Checksum error!\n");
			continue;
		}
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			if ((NULL != client_tcb[i]) && (client_tcb[i]->server_portNum == seg->header.src_port) && (client_tcb[i]->client_portNum == seg->header.dest_port) && (*src_nodeID == client_tcb[i]->server_nodeID)) {
				break;
			}
		}
		if (i == MAX_TRANSPORT_CONNECTIONS) {
			printf("the tcb you want to find does't exist!\n");
			continue;
		}
		switch (seg->header.type) {
			case 0:
				printf("the type stcp client receives is SYN\n");
				break;
			case 1:
				printf("the type stcp client receives is SYNACK\n");
				break;
			case 2:
				printf("the type stcp client receives is FIN\n");
				break;
			case 3:
				printf("the type stcp client receives is FINACK\n");
				break;
			case 4:
				printf("the type stcp client receives is DATA\n");
				break;
			case 5:
				printf("the type stcp client receives is DATAACK\n");
				break;
		}
		
		switch (client_tcb[i]->state) {
			case CLOSED:
				printf("stcp client now is in CLOSED state!\n");
				if (seg->header.type == SYN) {
					client_tcb[i]->state = SYNSENT;
				}
				break;
			case SYNSENT:
				printf("stcp client now is in SYNSENT state!\n");
				if (seg->header.type == SYNACK) {
					printf("receive a SYNACK!\n");
					client_tcb[i]->state = CONNECTED;
				}
				break;
			case CONNECTED:
				printf("stcp client now is in CONNECTED state!\n");
				if (seg->header.type == DATAACK) {
					printf("receive a DATAACK %d!\n", seg->header.ack_num);
					pthread_mutex_lock(client_tcb[i]->bufMutex);
					segBuf_t *p = client_tcb[i]->sendBufHead;
					while (p != NULL && p != client_tcb[i]->sendBufunSent) {
					    if (p->seg.header.seq_num < seg->header.ack_num) {
						   	client_tcb[i]->sendBufHead = p->next;
							printf("delete the %d seq_no\n", p->seg.header.seq_num);
							free(p);
							p = client_tcb[i]->sendBufHead;
							client_tcb[i]->unAck_segNum--;
					    }
						else break;/////////////////////////////////
					}
					//不该重发
					/*p = client_tcb[i]->sendBufHead;
					while (p != NULL && p != client_tcb[i]->sendBufunSent) {//重发，unAck_segNum不用加一
						p->sentTime = getCurrentTime();
						sip_sendseg(sip_conn, &(p->seg));
						p = p->next;
					}*/
					while((client_tcb[i]->unAck_segNum < GBN_WINDOW) && (client_tcb[i]->sendBufunSent != NULL)) {
						client_tcb[i]->sendBufunSent->sentTime = getCurrentTime();
						sip_sendseg(sip_conn, client_tcb[i]->server_nodeID, &(client_tcb[i]->sendBufunSent->seg));
						client_tcb[i]->sendBufunSent = client_tcb[i]->sendBufunSent->next;
						client_tcb[i]->unAck_segNum++;
					}
					pthread_mutex_unlock(client_tcb[i]->bufMutex);
				}
				break;
			case FINWAIT:
				printf("stcp client now is in FINWAIT state!\n");
				if (seg->header.type == FINACK) {
					printf("receive a FINACK!\n");
					client_tcb[i]->state = CLOSED;
				}
				break;
		}
	}
	printf("we are now off seghandler!\n");
  	return 0;
}


//这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
//如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
//当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
void* sendBuf_timer(void* clienttcb) 
{
	printf("we are now in sendBuf_timer!\n");
	long time;
	client_tcb_t *client_tcb = (client_tcb_t *)clienttcb;
	while (client_tcb->sendBufHead != NULL) {
		pthread_mutex_lock(client_tcb->bufMutex);
		time = getCurrentTime() - client_tcb->sendBufHead->sentTime;
		if (time * 1000000 > DATA_TIMEOUT) {
			printf("the sendBuf_timer thread resend the seg which is already sent!\n");
			segBuf_t *p = client_tcb->sendBufHead;
			while (p != NULL && p != client_tcb->sendBufunSent) {
				p->sentTime = getCurrentTime();
				sip_sendseg(sip_conn, client_tcb->server_nodeID, &(p->seg));
				p = p->next;
			}
		}
		pthread_mutex_unlock(client_tcb->bufMutex);
		usleep(SENDBUF_POLLING_INTERVAL / 1000);
	}
	printf("The sendBuf_timer thread is over!\n");
	return 0;
}

long getCurrentTime() {
	struct timeval tv;    
	gettimeofday(&tv,NULL);
	return (tv.tv_sec % 10000) * 1000 + tv.tv_usec / 1000;
}