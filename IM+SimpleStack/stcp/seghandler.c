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
#include "../common/seg.h"
#include "seghandler.h"
#include "stcp_client.h"
#include "stcp_server.h"

/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/


// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.

void* seghandler_client(int* src_nodeID, seg_t* seg, long i, long sip_conn) 
{		
	switch (client_tcb[i]->state) {
		case CLOSED:
			//printf("stcp client now is in CLOSED state!\n");
			if (seg->header.type == SYN) {
				client_tcb[i]->state = SYNSENT;
			}
			break;
		case SYNSENT:
			//printf("stcp client now is in SYNSENT state!\n");
			if (seg->header.type == SYNACK) {
				//printf("receive a SYNACK!\n");
				client_tcb[i]->state = CONNECTED;
			}
			break;
		case CONNECTED:
			//printf("stcp client now is in CONNECTED state!\n");
			if (seg->header.type == DATAACK) {
				//printf("receive a DATAACK %d!\n", seg->header.ack_num);
				pthread_mutex_lock(client_tcb[i]->bufMutex);
				segBuf_t *p = client_tcb[i]->sendBufHead;
				while (p != NULL && p != client_tcb[i]->sendBufunSent) {
					if (p->seg.header.seq_num < seg->header.ack_num) {
						client_tcb[i]->sendBufHead = p->next;
						//printf("delete the %d seq_no\n", p->seg.header.seq_num);
						free(p);
						p = client_tcb[i]->sendBufHead;
						client_tcb[i]->unAck_segNum--;
					}
					else break;/////////////////////////////////
				}
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
			//printf("stcp client now is in FINWAIT state!\n");
			if (seg->header.type == FINACK) {
				//printf("receive a FINACK!\n");
				client_tcb[i]->state = CLOSED;
			}
			break;
	}
  	return 0;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
void* seghandler_server(int* src_nodeID, seg_t* seg, long i, long sip_conn) 
{	
	//printf("the server_tcb is %d\n", i);
	//printf("the state is %d\n", server_tcb[i]->state);
	unsigned int client_port;
	client_port = seg->header.src_port;
	switch (server_tcb[i]->state) {
		case CLOSED:
			//printf("stcp server now is in CLOSED state!\n");
			break;
		case LISTENING:
			//printf("stcp server now is in LISTENING state!\n");
			if (seg->header.type == SYN) {
				//printf("receive a SYN!\n");
				server_tcb[i]->state = CONNECTED;
				server_tcb[i]->client_portNum = seg->header.src_port;
				server_tcb[i]->client_nodeID = *src_nodeID;
				sendACK(i, client_port, *src_nodeID, SYNACK, seg);
			}
			break;
		case CONNECTED:
			//printf("stcp server now is in CONNECTED state!\n");
			if (seg->header.type == SYN) {
				//printf("receive a SYN!\n");
				server_tcb[i]->state = CONNECTED;
				server_tcb[i]->expect_seqNum = seg->header.seq_num;
				sendACK(i, client_port, *src_nodeID, SYNACK, seg);
			}
			else if (seg->header.type == FIN) {
				//printf("receive a FIN!\n");
				server_tcb[i]->state = CLOSEWAIT;
				sendACK(i, client_port, *src_nodeID, FINACK, seg);
				pthread_t FINhandle_thread;
				int rc;
				rc = pthread_create(&FINhandle_thread, NULL, FINhandler, (void *)i);
				if (rc) {
					printf("ERROR; return code from pthread_create() is %d\n", rc);
					exit(-1);
				}	
			}
			else if (seg->header.type == DATA) {
				//printf("receive a DATA!\n");
				////printf("the expect_seqNum is %d\n", server_tcb[i]->expect_seqNum);
				//printf("the seqNum is %d\n", seg->header.seq_num);
				if (seg->header.seq_num == server_tcb[i]->expect_seqNum) {
				//	printf("the expect_seqNum == seq_num!\n");
					pthread_mutex_lock(server_tcb[i]->bufMutex);
					memcpy(server_tcb[i]->recvBuf + server_tcb[i]->usedBufLen, seg->data, seg->header.length);
				//	printf("the seg->header.length is %d\n", seg->header.length);
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
					sip_sendseg(sip_conn, *src_nodeID, seg);
				//	printf("stcp server send the changing DATAACK %d succesfully!\n", seg->header.ack_num);
					pthread_mutex_unlock(server_tcb[i]->bufMutex);
				}
				else {
				//	printf("the expect_seqNum != seq_num!\n");
					seg->header.src_port = server_tcb[i]->server_portNum;        //源端口号
					seg->header.dest_port = client_port;       //目的端口号
					seg->header.seq_num = 0;         //序号
					seg->header.ack_num = server_tcb[i]->expect_seqNum;         //确认号
					seg->header.length = 0;    //段数据长度
					seg->header.type = DATAACK;     //段类型
					seg->header.rcv_win = 0;  //当前未使用
					seg->header.checksum = 0;
					seg->header.checksum = checksum(seg);  //这个段的校验和
					sip_sendseg(sip_conn, *src_nodeID, seg);
				//	printf("stcp server send the not changed DATAACK %d succesfully!\n", seg->header.ack_num);
				}
			}
			break;
		case CLOSEWAIT:
			//printf("stcp server now is in CLOSEWAIT state!\n");
			if (seg->header.type == FIN) {
				//printf("receive a FIN!\n");
				sendACK(i, client_port, *src_nodeID, FINACK, seg);
				//printf("has sent FINACK!\n");
			}
			break;
	}
  	return 0;
}

void* seghandler(void* arg)
{
	//printf("we are now in seghandler!\n");
	long i;
	int flag;
	int sip_conn = *((int*)arg);
	while (1) {
		seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
		int* src_nodeID = (int*)malloc(sizeof(int));
		flag = sip_recvseg(sip_conn, src_nodeID, seg);
		if (flag == 1) {//报文丢失
			//printf("the stcp client does'n receive a segment! segment is lost!\n");
			continue;
		}
		if (flag == -1) {//接收不到报文，线程停止
			//printf("can't receive anything in tcp level, the seghandler thread is going to end.\n");
			break;
		}
		if (checkchecksum(seg) == -1) {
			//printf("Checksum error!\n");
			continue;
		}
		/*switch (seg->header.type) {
			case 0:
				printf("the type stcp receives is SYN\n");
				break;
			case 1:
				printf("the type stcp receives is SYNACK\n");
				break;
			case 2:
				printf("the type stcp receives is FIN\n");
				break;
			case 3:
				printf("the type stcp receives is FINACK\n");
				break;
			case 4:
				printf("the type stcp receives is DATA\n");
				break;
			case 5:
				printf("the type stcp receives is DATAACK\n");
				break;
		}*/
		//printf("bbbbbbbbbbbbbbbbbbb\n");
		/*********************匹配client tcb*********************/
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			/*if (NULL != client_tcb[i]) {
				printf("the i is %d\n", i);
				printf("the srcport: %d %d\n", client_tcb[i]->server_portNum, seg->header.src_port);
				printf("the destport: %d %d\n", client_tcb[i]->client_portNum, seg->header.dest_port);
				printf("the nodeID: %d %d\n", client_tcb[i]->server_nodeID, *src_nodeID);
			}*/
			if ((NULL != client_tcb[i]) && (client_tcb[i]->server_portNum == seg->header.src_port) && (client_tcb[i]->client_portNum == seg->header.dest_port) && (*src_nodeID == client_tcb[i]->server_nodeID)) {
				break;
			}
		}
		//printf("ccccccccccccccccccccc\n");
		if (i == MAX_TRANSPORT_CONNECTIONS) {
			//printf("the client tcb you want to find does't exist!\n");
			//continue;
		}
		else {
			//printf("find a client tcb!\n");
			seghandler_client(src_nodeID, seg, i, sip_conn);
			continue;
		}
		//printf("1111111111111111\n");
		/*********************匹配server tcb*********************/
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			if ((NULL != server_tcb[i]) && (server_tcb[i]->server_portNum == seg->header.dest_port) && (server_tcb[i]->client_portNum == seg->header.src_port) && (*src_nodeID == server_tcb[i]->client_nodeID)) {
				break;
			}
		}
		//printf("222222222222222222\n");
		if (i == MAX_TRANSPORT_CONNECTIONS) {
			//printf("the server tcb you want to find does't exist! but...\n");
			if (seg->header.type == SYN) {
				//printf("333333333333333333\n");
				for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
					if ((NULL != server_tcb[i]) && (server_tcb[i]->server_portNum == seg->header.dest_port) && (server_tcb[i]->state == LISTENING)) {
						break;
					}
				}
				
				if (i == MAX_TRANSPORT_CONNECTIONS) {
					//printf("the server tcb you want does't exist really!!\n");
					continue;
				}
				else {
					//printf("find a server tcb!\n");
					seghandler_server(src_nodeID, seg, i, sip_conn);
				}
				//printf("44444444444444444444\n");
			}
			else {
				//printf("555555555555555555555\n");
				continue;
			}
		}
		else {
			//printf("66666666666666666666666666\n");
			seghandler_server(src_nodeID, seg, i, sip_conn);
		}
	}
	return 0;
}
