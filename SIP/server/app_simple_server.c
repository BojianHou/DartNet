//�ļ���: server/app_simple_server.c

//����: ���Ǽ򵥰汾�ķ������������. �������������ӵ�����SIP����. Ȼ��������stcp_server_init()��ʼ��STCP������. 
//��ͨ�����ε���stcp_server_sock()��stcp_server_accept()����2���׽��ֲ��ȴ����Կͻ��˵�����. ������Ȼ����������������ӵĿͻ��˷��͵Ķ��ַ���. 
//���, ������ͨ������stcp_server_close()�ر��׽���, ���Ͽ��뱾��SIP���̵�����.

//��������: 2013��1��

//����: ��

//���: STCP������״̬

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
#include "../common/constants.h"
#include "stcp_server.h"

//������������, һ��ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. ��һ��ʹ�ÿͻ��˶˿ں�89�ͷ������˿ں�90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//�ڽ��յ��ַ�����, �ȴ�15��, Ȼ��ر�����.
#define WAITTIME 15

//����������ӵ�����SIP���̵Ķ˿�SIP_PORT. ���TCP����ʧ��, ����-1. ���ӳɹ�, ����TCP�׽���������, STCP��ʹ�ø����������Ͷ�.
int connectToSIP() {

	//Create a socket for the client
	//If sockfd<0 there was an error in the creation of the socket
	
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
	servaddr.sin_port = htons(SIP_PORT); //convert to big-endian order
	//Connection of the client to the socket
	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
		perror("Problem in connnecting to the server");
		exit(3);
	}
	system("clear");
	printf("Connect to the server successfully!\n");
	return sockfd;
	
}

//��������Ͽ�������SIP���̵�TCP����. 
void disconnectToSIP(int sip_conn) {

	//����Ҫ��д����Ĵ���.
	close(sip_conn);
	printf("Close the TCP connection between the STCP and the SIP!\n");
}

int main() {
	//���ڶ����ʵ����������
	srand(time(NULL));

	//���ӵ�SIP���̲����TCP�׽���������
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//��ʼ��STCP������
	stcp_server_init(sip_conn);

	//�ڶ˿�SERVERPORT1�ϴ���STCP�������׽��� 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//��������������STCP�ͻ��˵����� 
	stcp_server_accept(sockfd);

	//�ڶ˿�SERVERPORT2�ϴ�����һ��STCP�������׽���
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//��������������STCP�ͻ��˵����� 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//�������Ե�һ�����ӵ��ַ���
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//�������Եڶ������ӵ��ַ���
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd2,buf2,7);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//�ر�STCP������ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//�Ͽ���SIP����֮�������
	disconnectToSIP(sip_conn);
}
