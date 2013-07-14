// �ļ��� pkt.c
// ��������: 2013��1��

#include "pkt.h"
#include "stdio.h"
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

extern int errno;

// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	sendpkt_arg_t* arg = (sendpkt_arg_t*)malloc(sizeof(sendpkt_arg_t));
	arg->nextNodeID = nextNodeID;
	memcpy(&(arg->pkt), pkt, sizeof(sip_pkt_t));

	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(son_conn, sendbuf, 2, 0) < 0) {
		//printf("111111111111111111111\n");
		printf("send failure!\n");
		return -1;
	}
	
	if (send(son_conn, arg, sizeof(sendpkt_arg_t), 0) < 0) {
		//printf("222222222222222222222\n");
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(son_conn, sendbuf, 2, 0) < 0) {
		//printf("333333333333333333333\n");
		printf("send failure!\n");
		return -1;
	}
	//printf("we are now off son_sendpkt\n");
	return 1;
}

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(son_conn, &temp, 1, 0) < 0) {
			printf("son_recvpkt error no%d\n\n\n", errno);
			return -1;
			//continue;
		}
		switch (state) {
			case SEGSTART1:
				if (temp == '!') {
					state = SEGSTART2;
				}
				break;
			case SEGSTART2:
				if (temp == '&') {
					state = SEGRECV;
				}
				break;
			case SEGRECV:
				if (temp == '!') {
					state = SEGSTOP1;
				}
				else {
					recvbuf[i] = temp;
					i++;
				}
				break;
			case SEGSTOP1:
				if (temp == '#') {
					flag = 1;
				}
				else {//����'!#'��ϣ���Ҫ��'!'�������ַ��ŵ�������
					recvbuf[i] = '!';
					i++;
					if (temp == '!') {
						state = SEGSTOP1;
					}
					else {
						recvbuf[i] = temp;
						i++;
						state = SEGRECV;
					}
					
				}
				break;
		}
		if (flag == 1) {
			memcpy(pkt, recvbuf, sizeof(sip_pkt_t));
			break;
		}
	}
	//printf("we are now off son_recvpkt\n");
	return 1;
}

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	sendpkt_arg_t* arg = (sendpkt_arg_t*)malloc(sizeof(sendpkt_arg_t));

	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(sip_conn, &temp, 1, 0) < 0) {
			printf("getpktToSend error no%d\n\n\n", errno);
			return -1;
			//continue;
		}
		switch (state) {
			case SEGSTART1:
				if (temp == '!') {
					state = SEGSTART2;
				}
				break;
			case SEGSTART2:
				if (temp == '&') {
					state = SEGRECV;
				}
				break;
			case SEGRECV:
				if (temp == '!') {
					state = SEGSTOP1;
				}
				else {
					recvbuf[i] = temp;
					i++;
				}
				break;
			case SEGSTOP1:
				if (temp == '#') {
					flag = 1;
				}
				else {//����'!#'��ϣ���Ҫ��'!'�������ַ��ŵ�������
					recvbuf[i] = '!';
					i++;
					if (temp == '!') {
						state = SEGSTOP1;
					}
					else {
						recvbuf[i] = temp;
						i++;
						state = SEGRECV;
					}
				}
				break;
		}
		if (flag == 1) {
			memcpy(arg, recvbuf, sizeof(sendpkt_arg_t));
			*nextNode = arg->nextNodeID;
			memcpy(pkt, &(arg->pkt), sizeof(sip_pkt_t));
			break;
		}
	}
	//printf("we are now off getpktToSend\n");
	return 1;
}

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(sip_conn, sendbuf, 2, 0) < 0) {
		//printf("44444444444444444444444\n");
		printf("send failure!\n");
		return -1;
	}
	
	if (send(sip_conn, pkt, sizeof(sip_pkt_t), 0) < 0) {
		//printf("555555555555555555555555\n");
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(sip_conn, sendbuf, 2, 0) < 0) {
		//printf("6666666666666666666666666\n");
		printf("send failure!\n");
		return -1;
	}
	//printf("we are now off forwardpktToSIP\n");
	return 1;
}

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(conn, sendbuf, 2, 0) < 0) {
		//printf("77777777777777777777777\n");
		printf("send failure!\n");
		return -1;
	}
	
	if (send(conn, pkt, sizeof(sip_pkt_t), 0) < 0) {
		//printf("888888888888888888888888\n");
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(conn, sendbuf, 2, 0) < 0) {
		//printf("999999999999999999999999\n");
		printf("send failure!\n");
		return -1;
	}
	//printf("we are now off sendpkt\n");
	return 1;
}

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(conn, &temp, 1, 0) < 0) {
			printf("recvpkt error no%d\n\n\n", errno);
			return -1;
			//continue;
		}
		switch (state) {
			case SEGSTART1:
				if (temp == '!') {
					state = SEGSTART2;
				}
				break;
			case SEGSTART2:
				if (temp == '&') {
					state = SEGRECV;
				}
				break;
			case SEGRECV:
				if (temp == '!') {
					state = SEGSTOP1;
				}
				else {
					recvbuf[i] = temp;
					i++;
				}
				break;
			case SEGSTOP1:
				if (temp == '#') {
					flag = 1;
				}
				else {//����'!#'��ϣ���Ҫ��'!'�������ַ��ŵ�������
					recvbuf[i] = '!';
					i++;
					if (temp == '!') {
						state = SEGSTOP1;
					}
					else {
						recvbuf[i] = temp;
						i++;
						state = SEGRECV;
					}
				}
				break;
		}
		if (flag == 1) {
			memcpy(pkt, recvbuf, sizeof(sip_pkt_t));
			break;
		}
	}
	//printf("we are now off recvpkt\n");
	return 1;
}
