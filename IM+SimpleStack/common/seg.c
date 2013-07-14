
#include "seg.h"
#include "stdio.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

extern int errno;

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
	sendseg_arg_t* arg = (sendseg_arg_t*)malloc(sizeof(sendseg_arg_t));
	arg->nodeID = dest_nodeID;
	memcpy(&(arg->seg), segPtr, sizeof(seg_t));

	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(sip_conn, sendbuf, 2, 0) < 0) {
		printf("send failure!\n");
		return -1;
	}
	
	if (send(sip_conn, arg, sizeof(sendseg_arg_t), 0) < 0) {
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(sip_conn, sendbuf, 2, 0) < 0) {
		printf("send failure!\n");
		return -1;
	}
	
  	return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
	sendseg_arg_t* arg = (sendseg_arg_t*)malloc(sizeof(sendseg_arg_t));
	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(sip_conn, &temp, 1, 0) <= 0) {//必须是小于等于0
			printf("sip_recvseg error no%d\n\n\n", errno);
			return -1;
			//continue;
		}
		/*if (recv(sip_conn, &temp, 1, 0) == 0) {
			printf("sip_recvseg opposite stop error no%d\n\n\n", errno);
			return -1;
		}*/
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
				else {//不是'!#'组合，则要把'!'及其后的字符放到缓存里
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
			memcpy(arg, recvbuf, sizeof(sendseg_arg_t));
			*src_nodeID = arg->nodeID;
			memcpy(segPtr, &(arg->seg), sizeof(seg_t));
			break;
		}
	}
	/*if (seglost(segPtr) == 1) {//包丢失
		return 1;
	}*/
	return 0;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
	sendseg_arg_t* arg = (sendseg_arg_t*)malloc(sizeof(sendseg_arg_t));
	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(stcp_conn, &temp, 1, 0) <= 0) {//必须是小于等于0
			printf("getsegToSend error no%d\n\n\n", errno);
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
				else {//不是'!#'组合，则要把'!'及其后的字符放到缓存里
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
			memcpy(arg, recvbuf, sizeof(sendseg_arg_t));
			*dest_nodeID = arg->nodeID;
			memcpy(segPtr, &(arg->seg), sizeof(seg_t));
			break;
		}
	}
	return 1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
	sendseg_arg_t* arg = (sendseg_arg_t*)malloc(sizeof(sendseg_arg_t));
	arg->nodeID = src_nodeID;
	memcpy(&(arg->seg), segPtr, sizeof(seg_t));

	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(stcp_conn, sendbuf, 2, 0) < 0) {
		printf("send failure!\n");
		return -1;
	}
	
	if (send(stcp_conn, arg, sizeof(sendseg_arg_t), 0) < 0) {
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(stcp_conn, sendbuf, 2, 0) < 0) {
		printf("send failure!\n");
		return -1;
	}
	
  	return 1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
			return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			printf("revert some bit!!!\n");
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
	unsigned short *buffer = (unsigned short *)segment;
	int size = segment->header.length + 24;
	unsigned long cksum = 0;
	while (size > 1) {
		cksum += *buffer++;
		size -= sizeof(unsigned short);
	}
	if (size) {//假如还有剩余的字节，也加上
		cksum += *(unsigned char*)buffer * 256;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff); //将高16bit与低16bit相加
	while(cksum >> 16) {//反复将cksum高位和低位相加，直到高位为0
		cksum = (cksum & 0xffff) + (cksum >> 16);
	}
	return (unsigned short)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
	if (segment->header.length > MAX_SEG_LEN) {
		return -1;
	}
	if (checksum(segment) == 0) {
		return 1;
	}
	return -1;
}
