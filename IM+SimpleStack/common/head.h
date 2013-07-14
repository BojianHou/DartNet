#ifndef HEAD_H
#define HEAD_H

#define MAXLINE 800 /*max text line length*/
#define SERV_PORT 5050 /*port*/
#define NUM_THREADS 8 /*maximum number of client threads*/
#define NAMESIZE 100

#define LOGIN 0
#define ONLINEFRI 1
#define MESSAGE 2
#define SENDTOALL 3
#define LOGOFF 4
#define REFUSED 5
#define LOGED 6
#define BUSY 7
#define INFORM 8
#define NOTEXIST 9
#define INFINITE 999

char *friendlist[NUM_THREADS];//online friendlist
int sockfdlist[NUM_THREADS];//to store every sockfd
int sockfdlist2[NUM_THREADS];

struct packet_IM {
	int service;//include authentic,message,logoff
	int status;
	char srcname[100];
	char desname[100];
	char message[MAXLINE];
};
#endif
