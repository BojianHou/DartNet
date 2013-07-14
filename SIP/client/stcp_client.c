//�ļ���: client/stcp_client.c
//
//����: ����ļ�����STCP�ͻ��˽ӿ�ʵ�� 
//
//��������: 2013��1��

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

//����tcbtableΪȫ�ֱ���
client_tcb_t* client_tcb[MAX_TRANSPORT_CONNECTIONS];
//������SIP���̵�TCP����Ϊȫ�ֱ���
int sip_conn;

/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳�����������STCP��. �ͻ���ֻ��һ��seghandler.
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

// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
int stcp_client_sock(unsigned int client_port) 
{
	printf("----------------stcp client sock start!----------------\n");
  	int i;
  	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
  		if (client_tcb[i] == NULL) {
			printf("find a NULL tcb, initial it!\n");
  			client_tcb[i] = (client_tcb_t *)malloc(sizeof(struct client_tcb));
			client_tcb[i]->server_nodeID = 0;        //�������ڵ�ID, ����IP��ַ
			client_tcb[i]->server_portNum = 0;       //�������˿ں�
			client_tcb[i]->client_nodeID = 0;     //�ͻ��˽ڵ�ID, ����IP��ַ
			client_tcb[i]->client_portNum = client_port;    //�ͻ��˶˿ں�
			client_tcb[i]->state = CLOSED;     	//�ͻ���״̬
			client_tcb[i]->next_seqNum = 1;       //�¶�׼��ʹ�õ���һ����� 
			client_tcb[i]->bufMutex = malloc(sizeof(pthread_mutex_t));      //���ͻ�����������
			client_tcb[i]->sendBufHead = NULL;          //���ͻ�����ͷ
			client_tcb[i]->sendBufunSent = NULL;        //���ͻ������еĵ�һ��δ���Ͷ�
			client_tcb[i]->sendBufTail = NULL;          //���ͻ�����β
			client_tcb[i]->unAck_segNum = 0;      //�ѷ��͵�δ�յ�ȷ�϶ε�����
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

// ��������������ӷ�����. �����׽���ID, �������ڵ�ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������ڵ�ID�ͷ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	printf("----------------stcp client connect start!----------------\n");
  	int tryno;
  	tryno = 0;
  	client_tcb[sockfd]->server_portNum = server_port;
	client_tcb[sockfd]->state = SYNSENT;
	client_tcb[sockfd]->server_nodeID = nodeID;
	
	seg_t *seg = (seg_t*)malloc(sizeof(seg_t));

	seg->header.src_port = client_tcb[sockfd]->client_portNum;        //Դ�˿ں�
	seg->header.dest_port = server_port;       //Ŀ�Ķ˿ں�
	seg->header.seq_num = client_tcb[sockfd]->next_seqNum;         //���
	seg->header.ack_num = 0;         //ȷ�Ϻ�
	seg->header.length = 0;    //�����ݳ���
	seg->header.type = SYN;     //������
	seg->header.rcv_win = 0;  //��ǰδʹ��
	seg->header.checksum = 0;
	seg->header.checksum = checksum(seg);  //����ε�У���
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

// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ.
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������.
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����.
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����. 
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
// stcp_client_send��һ����������������.
// ��Ϊ�û����ݱ���ƬΪ�̶���С��STCP��, ����һ��stcp_client_send���ÿ��ܻ�������segBuf
// ����ӵ����ͻ�����������. ������óɹ�, ���ݾͱ�����TCB���ͻ�����������, ���ݻ������ڵ����,
// ���ݿ��ܱ����䵽������, ���ڶ����еȴ�����.

int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
    pthread_mutex_lock(client_tcb[sockfd]->bufMutex);
	while (length > 0) {
		segBuf_t *segbuf = (segBuf_t*)malloc(sizeof(segBuf_t));
		
		segbuf->seg.header.src_port = client_tcb[sockfd]->client_portNum;        //Դ�˿ں�
		segbuf->seg.header.dest_port = client_tcb[sockfd]->server_portNum;       //Ŀ�Ķ˿ں�
		segbuf->seg.header.seq_num = client_tcb[sockfd]->next_seqNum;         //���
		segbuf->seg.header.ack_num = 0;         //ȷ�Ϻ�
		segbuf->seg.header.type = DATA;     //������
		segbuf->seg.header.rcv_win = 0;  //��ǰδʹ��

		if (length > MAX_SEG_LEN) {
			segbuf->seg.header.length = MAX_SEG_LEN;    //�����ݳ���
			memcpy(segbuf->seg.data, data, MAX_SEG_LEN);
			data = (char*)data;
			data += MAX_SEG_LEN;
			length -= MAX_SEG_LEN;
		}
		else {
			segbuf->seg.header.length = length;    //�����ݳ���
			memcpy(segbuf->seg.data, data, length);
			length -= length;
		}
		
		segbuf->seg.header.checksum = 0;
		segbuf->seg.header.checksum = checksum(&(segbuf->seg));  //����ε�У���

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

			/* ���뵽�������� */
			client_tcb[sockfd]->sendBufHead = segbuf;
			client_tcb[sockfd]->sendBufunSent = segbuf;
			client_tcb[sockfd]->sendBufTail = segbuf;
			//����
			sip_sendseg(sip_conn, client_tcb[sockfd]->server_nodeID, &(segbuf->seg));
			printf("the segbuf->seg.header.length is %d and the seqno is %d\n", segbuf->seg.header.length, segbuf->seg.header.seq_num);
			client_tcb[sockfd]->sendBufunSent = client_tcb[sockfd]->sendBufunSent->next;
			client_tcb[sockfd]->unAck_segNum++;
		}
		else {
			//���뵽������β��
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
			else {//sendBufunSent��Ϊnull�����sendBufunSent��ʼ���ͺ�����
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

// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.

int stcp_client_disconnect(int sockfd) 
{
	while(client_tcb[sockfd]->sendBufHead != NULL) {
		usleep(100000);
	}
	printf("----------------stcp client disconnect start!----------------\n");
	
  	int tryno;
  	tryno = 0;
	
	seg_t *seg = (seg_t*)malloc(sizeof(seg_t));

	seg->header.src_port = client_tcb[sockfd]->client_portNum;        //Դ�˿ں�
	seg->header.dest_port = client_tcb[sockfd]->server_portNum;       //Ŀ�Ķ˿ں�
	seg->header.seq_num = client_tcb[sockfd]->next_seqNum;         //���
	seg->header.ack_num = 0;         //ȷ�Ϻ�
	seg->header.length = 0;    //�����ݳ���
	seg->header.type = FIN;     //������
	seg->header.rcv_win = 0;  //��ǰδʹ��
	seg->header.checksum = 0;
	seg->header.checksum = checksum(seg);  //����ε�У���

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
			while (p != NULL) {//��ջ�����
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
	while (p != NULL) {//��ջ�����
		client_tcb[sockfd]->sendBufHead = p->next;
		free(p);
		p = client_tcb[sockfd]->sendBufHead;
		client_tcb[sockfd]->unAck_segNum--;
	}
  	return 1;
}

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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

// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
void* seghandler(void* arg) 
{
	printf("we are now in seghandler!\n");
	int i;
	int flag;
	while (1) {
		seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
		int* src_nodeID = (int*)malloc(sizeof(int));
		flag = sip_recvseg(sip_conn, src_nodeID, seg);
		if (flag == 1) {//���Ķ�ʧ
			printf("the stcp client does'n receive a segment! segment is lost!\n");
			continue;
		}
		if (flag == -1) {//���ղ������ģ��߳�ֹͣ
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
					//�����ط�
					/*p = client_tcb[i]->sendBufHead;
					while (p != NULL && p != client_tcb[i]->sendBufunSent) {//�ط���unAck_segNum���ü�һ
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


//����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
//���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
//����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
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