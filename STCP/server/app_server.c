//�ļ���: server/app_server.c

//����: ���Ƿ�����Ӧ�ó������. ����������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص������. Ȼ��������stcp_server_init()��ʼ��STCP������. 
//��ͨ�����ε���stcp_server_sock()��stcp_server_accept()����2���׽��ֲ��ȴ����Կͻ��˵�����. ���, ������ͨ������stcp_server_close()�ر��׽���.
//�ص������ͨ������son_stop()ֹͣ.

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

//������������, һ��ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. ��һ��ʹ�ÿͻ��˶˿ں�89�ͷ������˿ں�90
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//�����Ӵ�����, �ȴ�10��, Ȼ��ر�����
#define WAITTIME 10

//�������ͨ���ڿͻ��ͷ�����֮�䴴��TCP�����������ص������. ������TCP�׽���������, STCP��ʹ�ø����������Ͷ�. ���TCP����ʧ��, ����-1.
int son_start() {
}

//�������ͨ���رտͻ��ͷ�����֮���TCP������ֹͣ�ص������
void son_stop(int son_conn) {
}

int main() {
	//���ڶ����ʵ����������
	srand(time(NULL));

	//�����ص�����㲢��ȡ�ص������TCP�׽���������
	int son_conn = son_start();
	if(son_conn<0) {
		printf("can not start overlay network\n");
	}

	//��ʼ��STCP������
	stcp_server_init(son_conn);
	
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

	//ֹͣ�ص������
	son_stop(son_conn);
}
