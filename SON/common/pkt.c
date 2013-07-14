// 文件名: common/pkt.c
// 创建日期: 2013年1月

#include "pkt.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include "stdio.h"

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	sendpkt_arg_t* arg = (sendpkt_arg_t*)malloc(sizeof(sendpkt_arg_t));
	arg->nextNodeID = nextNodeID;
	memcpy(&(arg->pkt), pkt, sizeof(sip_pkt_t));

	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(son_conn, sendbuf, 2, 0) <= 0) {
		//printf("111111111111111111111\n");
		printf("send failure!\n");
		return -1;
	}
	
	if (send(son_conn, arg, sizeof(sendpkt_arg_t), 0) <= 0) {
		//printf("222222222222222222222\n");
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(son_conn, sendbuf, 2, 0) <= 0) {
		//printf("333333333333333333333\n");
		printf("send failure!\n");
		return -1;
	}
	printf("we are now off son_sendpkt\n");
	return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	//printf("we are now in son_recvpkt\n");
	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(son_conn, &temp, 1, 0) <= 0) {
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
			memcpy(pkt, recvbuf, sizeof(sip_pkt_t));
			break;
		}
	}
	printf("we are now off son_recvpkt\n");
	return 1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	//printf("we are now in getpktToSend\n");
	sendpkt_arg_t* arg = (sendpkt_arg_t*)malloc(sizeof(sendpkt_arg_t));

	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(sip_conn, &temp, 1, 0) <= 0) {
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
			memcpy(arg, recvbuf, sizeof(sendpkt_arg_t));
			*nextNode = arg->nextNodeID;
			memcpy(pkt, &(arg->pkt), sizeof(sip_pkt_t));
			break;
		}
	}
	printf("we are now off getpktToSend\n");
	return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	//printf("we are now in forwardpktToSIP\n");
	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(sip_conn, sendbuf, 2, 0) <= 0) {
		//printf("44444444444444444444444\n");
		printf("send failure!\n");
		return -1;
	}
	
	if (send(sip_conn, pkt, sizeof(sip_pkt_t), 0) <= 0) {
		//printf("555555555555555555555555\n");
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(sip_conn, sendbuf, 2, 0) <= 0) {
		//printf("6666666666666666666666666\n");
		printf("send failure!\n");
		return -1;
	}
	printf("we are now off forwardpktToSIP\n");
	return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	//printf("we are now in sendpkt\n");
	char sendbuf[3];
	sendbuf[0] = '!';
	sendbuf[1] = '&';
	
	if (send(conn, sendbuf, 2, 0) <= 0) {
		//printf("77777777777777777777777\n");
		printf("send failure!\n");
		return -1;
	}
	
	if (send(conn, pkt, sizeof(sip_pkt_t), 0) <= 0) {
		//printf("888888888888888888888888\n");
		printf("send failure!\n");
		return -1;
	}
	
	sendbuf[0] = '!';
	sendbuf[1] = '#';
	if (send(conn, sendbuf, 2, 0) <= 0) {
		//printf("999999999999999999999999\n");
		printf("send failure!\n");
		return -1;
	}
	printf("we are now off sendpkt\n");
	return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	//printf("we are now in recvpkt\n");
	int i = 0;
	int flag = 0;
	int state = SEGSTART1;
	char temp;
	char recvbuf[BUF_SIZE];
	while (1) {
		if (recv(conn, &temp, 1, 0) <= 0) {
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
			memcpy(pkt, recvbuf, sizeof(sip_pkt_t));
			break;
		}
	}
	printf("we are now off recvpkt\n");
	return 1;
}
