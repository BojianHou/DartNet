//�ļ���: client/app_stress_client.c
//
//����: ����ѹ�����԰汾�Ŀͻ��˳������. �ͻ�������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص������.
//Ȼ��������stcp_client_init()��ʼ��STCP�ͻ���. ��ͨ������stcp_client_sock()��stcp_client_connect()�����׽��ֲ����ӵ�������.
//Ȼ������ȡ�ļ�sendthis.txt�е��ı�����, ���ļ��ĳ��Ⱥ��ļ����ݷ��͸�������. 
//����һ��ʱ���, �ͻ��˵���stcp_client_disconnect()�Ͽ���������������. 
//���,�ͻ��˵���stcp_client_close()�ر��׽���. �ص������ͨ������son_stop()ֹͣ.

//��������: 2013��1��

//����: ��

//���: STCP�ͻ���״̬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/constants.h"
#include "stcp_client.h"

//����һ������, ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88

//�ڷ����ļ���, �ȴ�5��, Ȼ��ر�����.
#define WAITTIME 5

//�������ͨ���ڿͻ��ͷ�����֮�䴴��TCP�����������ص������. ������TCP�׽���������, STCP��ʹ�ø����������Ͷ�. ���TCP����ʧ��, ����-1. 
int son_start() {
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
	servaddr.sin_addr.s_addr = inet_addr("114.212.190.186");//connect to the server
	servaddr.sin_port = htons(SON_PORT); //convert to big-endian order
	//Connection of the client to the socket
	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))<0) {
		perror("Problem in connnecting to the server");
		exit(3);
	}
	system("clear");
	printf("Connect to the server successfully!\n");
	return sockfd;
	//����Ҫ��д����Ĵ���

}

//�������ͨ���رտͻ��ͷ�����֮���TCP������ֹͣ�ص������
void son_stop(int son_conn) {
	close(son_conn);
	printf("Close the TCP connection between the client and the server!\n");
	//����Ҫ��д����Ĵ���
}

int main() {
	//���ڶ����ʵ����������
	srand(time(NULL));

	//�����ص�����㲢��ȡ�ص������TCP�׽���������	
	int son_conn = son_start();
	if(son_conn<0) {
		printf("fail to start overlay network\n");
		exit(1);
	}

	//��ʼ��stcp�ͻ���
	stcp_client_init(son_conn);

	//�ڶ˿�87�ϴ���STCP�ͻ����׽���, �����ӵ�STCP�������˿�88.
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//��ȡsendthis.txt�ļ�����, ��������������ȡ�ļ��е�����.
	FILE *f;
	f = fopen("sendthis.txt","r");
	assert(f!=NULL);
	fseek(f,0,SEEK_END);
	int fileLen = ftell(f);
	fseek(f,0,SEEK_SET);
	char *buffer = (char*)malloc(fileLen);
	fread(buffer,fileLen,1,f);
	fclose(f);

	//���ȷ����ļ�����, Ȼ���������ļ�.
	stcp_client_send(sockfd,&fileLen,sizeof(int));
    stcp_client_send(sockfd, buffer, fileLen);
	free(buffer);

	//�ȴ�һ��ʱ��, Ȼ��ر�����.
	//sleep(WAITTIME);

	if(stcp_client_disconnect(sockfd)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}
	
	//ֹͣ�ص������
	son_stop(son_conn);
}
