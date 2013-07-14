//�ļ���: server/stcp_server.c
//
//����: ����ļ�����STCP�������ӿ�ʵ��. 
//
//��������: 2013��1��

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"
#include "../common/seghandler.h"

//������SIP���̵�����Ϊȫ�ֱ���
int sip_conn;

/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL. �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, 
// �ñ�����Ϊsip_sendseg��sip_recvseg���������. ���, �����������seghandler�߳�����������STCP��.
// ������ֻ��һ��seghandler.


void stcp_server_init(int conn) 
{
	//printf("----------------stcp server initial start!----------------\n");
	int i;
	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		server_tcb[i] = NULL;
	}
	
	sip_conn = conn;
	
	/*pthread_t seghandle_thread;
	int rc;
	rc = pthread_create(&seghandle_thread, NULL, seghandler, NULL);
	if (rc) {
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	printf("start the stcp server seghandler thread succesfully!\n");*/
	return;
}

// ����������ҷ�����TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��, ����, TCB state������ΪCLOSED, �������˿ڱ�����Ϊ�������ò���server_port. 
// TCB������Ŀ������Ӧ��Ϊ�����������׽���ID�������������, �����ڱ�ʶ�������˵�����. 
// ���TCB����û����Ŀ����, �����������-1.

int stcp_server_sock(unsigned int server_port) 
{
	//printf("----------------stcp server sock start!----------------\n");
	int i;
  	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
  		if (server_tcb[i] == NULL) {
			//printf("find a NULL tcb, initial it!\n");
  			server_tcb[i] = (server_tcb_t *)malloc(sizeof(struct server_tcb));
			server_tcb[i]->server_portNum = server_port;       //�������˿ں�
			server_tcb[i]->client_portNum = 0;    //�ͻ��˶˿ں�
			server_tcb[i]->state = CLOSED;     	//�ͻ���״̬
			server_tcb[i]->expect_seqNum = 1;       //�¶�׼��ʹ�õ���һ����� 

			server_tcb[i]->server_nodeID = 0;     //�������ڵ�ID, ����IP��ַ
			server_tcb[i]->server_portNum = server_port;    //�������˿ں�
			server_tcb[i]->client_nodeID = 0;     //�ͻ��˽ڵ�ID, ����IP��ַ
			server_tcb[i]->client_portNum = 0;    //�ͻ��˶˿ں�
			server_tcb[i]->state = CLOSED;         	//������״̬
			server_tcb[i]->expect_seqNum = 1;     //�������ڴ����������	
			server_tcb[i]->recvBuf = (char*)malloc(RECEIVE_BUF_SIZE);             //ָ����ջ�������ָ��
			server_tcb[i]->usedBufLen = 0;       //���ջ��������ѽ������ݵĴ�С
			server_tcb[i]->bufMutex = malloc(sizeof(pthread_mutex_t));      //ָ��һ����������ָ��, �û��������ڶԽ��ջ������ķ���
			if (pthread_mutex_init(server_tcb[i]->bufMutex, PTHREAD_MUTEX_TIMED_NP) !=0 ){
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

// �������ʹ��sockfd���TCBָ��, �������ӵ�stateת��ΪLISTENING. ��Ȼ��������ʱ������æ�ȴ�ֱ��TCB״̬ת��ΪCONNECTED 
// (���յ�SYNʱ, seghandler�����״̬��ת��). �ú�����һ������ѭ���еȴ�TCB��stateת��ΪCONNECTED,  
// ��������ת��ʱ, �ú�������1. �����ʹ�ò�ͬ�ķ�����ʵ�����������ȴ�.

int stcp_server_accept(int sockfd) 
{
	//printf("----------------stcp server accept start!----------------\n");
	server_tcb[sockfd]->state = LISTENING;
	while (server_tcb[sockfd]->state != CONNECTED) {
		usleep(SYN_TIMEOUT / 1000);
	}
	//printf("stcp server has accepted the client on %dth socket succesfully!\n", sockfd);
	return 1;
}

// ��������STCP�ͻ��˵�����. �������ÿ��RECVBUF_POLLING_INTERVALʱ��
// �Ͳ�ѯ���ջ�����, ֱ���ȴ������ݵ���, ��Ȼ��洢���ݲ�����1. ����������ʧ��, �򷵻�-1.

int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	//printf("----------------stcp server recv start!----------------\n");
	while (1) {
		sleep(RECVBUF_POLLING_INTERVAL);
		pthread_mutex_lock(server_tcb[sockfd]->bufMutex);
		//printf("the length is %d\n", length);
		
		//printf("the usedBufLen is: %d\n", server_tcb[sockfd]->usedBufLen);
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
int stcp_server_close(int sockfd) 
{
	while (server_tcb[sockfd]->usedBufLen != 0) {
		sleep(1);
	}
	//printf("----------------stcp server close start!----------------\n");
	//printf("server_tcb is %d\n", sockfd);
	sleep(CLOSEWAIT_TIMEOUT);
	if (server_tcb[sockfd]->state != CLOSED) {
		free(server_tcb[sockfd]);
		pthread_mutex_destroy(server_tcb[sockfd]->bufMutex);
		free(server_tcb[sockfd]->recvBuf);
		server_tcb[sockfd] = NULL;
		//printf("server %dth sockfd has closed not normally!\n", sockfd);
		return -1;
	}
	free(server_tcb[sockfd]);
	pthread_mutex_destroy(server_tcb[sockfd]->bufMutex);
	free(server_tcb[sockfd]->recvBuf);
	server_tcb[sockfd] = NULL;
	//printf("server %dth sockfd has closed normally!\n", sockfd);
	return 1;
}

// ������stcp_server_init()�������߳�. �������������Կͻ��˵Ľ�������. seghandler�����Ϊһ������sip_recvseg()������ѭ��, 
// ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�, �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���.
// ��鿴�����FSM���˽����ϸ��.
/*void* seghandler(void* arg) 
{
	long i;
	int flag;
	unsigned int client_port;
	seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
	int* src_nodeID = (int*)malloc(sizeof(int));
	while (1) {
		flag = sip_recvseg(sip_conn, src_nodeID, seg);///////////////////////////////////ע��src_nodeID��ʹ��
		if (flag == 1) {//���Ķ�ʧ
			printf("the stcp server does'n receive a segment! segment is lost!\n");
			continue;
		}
		if (checkchecksum(seg) == -1) {
			printf("Checksum error!\n");
			continue;
		}
		if (flag == -1) {//���ղ������ģ��߳�ֹͣ
			printf("can't receive anything in tcp level, the seghandler thread is going to end.\n");
			break;
		}
		for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
			if ((NULL != server_tcb[i]) && (server_tcb[i]->server_portNum == seg->header.dest_port) && (server_tcb[i]->client_portNum == seg->header.src_port) && (*src_nodeID == server_tcb[i]->client_nodeID)) {
				break;
			}
		}
		if (i == MAX_TRANSPORT_CONNECTIONS) {
			printf("the tcb you want to find does't exist! but...\n");
			if (seg->header.type == SYN) {
				for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
					if ((NULL != server_tcb[i]) && (server_tcb[i]->server_portNum == seg->header.dest_port)) {
						break;
					}
				}
				if (i == MAX_TRANSPORT_CONNECTIONS) {
					printf("the tcb you want does't exist really!!\n");
					continue;
				}
			}
			else
				continue;
		}
		switch (seg->header.type) {
			case 0:
				printf("the type stcp server receives is SYN\n");
				break;
			case 1:
				printf("the type stcp server receives is SYNACK\n");
				break;
			case 2:
				printf("the type stcp server receives is FIN\n");
				break;
			case 3:
				printf("the type stcp server receives is FINACK\n");
				break;
			case 4:
				printf("the type stcp server receives is DATA\n");
				break;
			case 5:
				printf("the type stcp server receives is DATAACK\n");
				break;
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
					server_tcb[i]->client_portNum = seg->header.src_port;
					server_tcb[i]->client_nodeID = *src_nodeID;
					sendACK(i, client_port, *src_nodeID, SYNACK, seg);
				}
				break;
			case CONNECTED:
				printf("stcp server now is in CONNECTED state!\n");
				if (seg->header.type == SYN) {
					printf("receive a SYN!\n");
					server_tcb[i]->state = CONNECTED;
					server_tcb[i]->expect_seqNum = seg->header.seq_num;
					sendACK(i, client_port, *src_nodeID, SYNACK, seg);
				}
				else if (seg->header.type == FIN) {
					printf("receive a FIN!\n");
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
						seg->header.src_port = server_tcb[i]->server_portNum;        //Դ�˿ں�
						seg->header.dest_port = client_port;       //Ŀ�Ķ˿ں�
						seg->header.seq_num = 0;         //���
						seg->header.ack_num = server_tcb[i]->expect_seqNum;         //ȷ�Ϻ�
						seg->header.length = 0;    //�����ݳ���
						seg->header.type = DATAACK;     //������
						seg->header.rcv_win = 0;  //��ǰδʹ��
						seg->header.checksum = 0;
						seg->header.checksum = checksum(seg);  //����ε�У���
						sip_sendseg(sip_conn, *src_nodeID, seg);
						printf("stcp server send the changing DATAACK %d succesfully!\n", seg->header.ack_num);
						pthread_mutex_unlock(server_tcb[i]->bufMutex);
					}
					else {
						printf("the expect_seqNum != seq_num!\n");
						seg->header.src_port = server_tcb[i]->server_portNum;        //Դ�˿ں�
						seg->header.dest_port = client_port;       //Ŀ�Ķ˿ں�
						seg->header.seq_num = 0;         //���
						seg->header.ack_num = server_tcb[i]->expect_seqNum;         //ȷ�Ϻ�
						seg->header.length = 0;    //�����ݳ���
						seg->header.type = DATAACK;     //������
						seg->header.rcv_win = 0;  //��ǰδʹ��
						seg->header.checksum = 0;
						seg->header.checksum = checksum(seg);  //����ε�У���
						sip_sendseg(sip_conn, *src_nodeID, seg);
						printf("stcp server send the not changed DATAACK %d succesfully!\n", seg->header.ack_num);
					}
				}
				break;
			case CLOSEWAIT:
				printf("stcp server now is in CLOSEWAIT state!\n");
				if (seg->header.type == FIN) {
					printf("receive a FIN!\n");
					sendACK(i, client_port, *src_nodeID, FINACK, seg);
				}
				break;
		}
	}
  	return 0;
}*/

void *FINhandler(void* index) {
	usleep(FIN_TIMEOUT / 1000);
	long i = (long)index;
	server_tcb[i]->state = CLOSED;
	return 0;
}

void sendACK(int index, unsigned int client_port, int dest_nodeID, int type, seg_t* seg) {
	//printf("the type stcp server sends is %d\n", type);

	seg->header.src_port = server_tcb[index]->server_portNum;        //Դ�˿ں�
	seg->header.dest_port = client_port;       //Ŀ�Ķ˿ں�
	seg->header.seq_num = 0;         //���
	seg->header.ack_num = server_tcb[index]->expect_seqNum;         //ȷ�Ϻ�
	seg->header.length = 0;    //�����ݳ���
	seg->header.type = type;     //������
	seg->header.rcv_win = 0;  //��ǰδʹ��
	seg->header.checksum = 0;
	seg->header.checksum = checksum(seg);  //����ε�У���

	server_tcb[index]->expect_seqNum++;
	sip_sendseg(sip_conn, dest_nodeID, seg);
	//printf("stcp server send the %d ACK succesfully!\n", type);
}

