//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年1月

#include "topology.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	struct hostent *h = gethostbyname(hostname);
	if (h == NULL) {
		printf("cannot get an addr!\n");
		return -1;
	}
	int nodeID;
	nodeID = h->h_addr_list[0][3] & 0x000000FF;
	printf("the %s's nodeID is: %d\n", hostname, nodeID);
	return nodeID;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	int nodeID = addr->s_addr >> 24;
	return nodeID;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	char name[32];
	gethostname(name, sizeof(name));
	struct hostent *h = gethostbyname(name);
	if (h == NULL) {
		printf("cannot get an addr!\n");
		return -1;
	}
	int nodeID;
	nodeID = h->h_addr_list[0][3] & 0x000000FF;
	printf("the %s's nodeID is: %d\n", name, nodeID);
	return nodeID;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	FILE *fp;
	if ((fp = fopen("../topology/topology.dat", "r")) == NULL) {
		printf("cannot open topology.dat!\n");
		exit(0);
	}
	int count = 0;
	int i;
	char hostname1[32], hostname2[32];
	int cost;
	char localname[32];
	gethostname(localname, sizeof(localname));
	for(i = 0; !feof(fp); ++i) {
		strcpy(hostname1, "vacant line");
		strcpy(hostname2, "vacant line");//to prevent the vacant line
		fscanf(fp, "%s %s %d", hostname1, hostname2, &cost);
		//printf("%s, %s, %d\n", hostname1, hostname2, cost);
		if (strcmp(hostname1, localname) == 0 || strcmp(hostname2, localname) == 0)
			count++;
	}
	fclose(fp);
	printf("the neighbour num is %d\n", count);
	return count;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
	return 4;//以后需要修改
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	int *nodeID = (int*)malloc(sizeof(int));
	nodeID[0] = 185;
	nodeID[1] = 186;
	nodeID[2] = 187;
	nodeID[3] = 188;
	return nodeID;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	int myNodeID = topology_getMyNodeID();
	int *nodeID = (int*)malloc(sizeof(int));
	if (myNodeID == 185) {
		nodeID[0] = 186;
		nodeID[1] = 187;
		nodeID[2] = 188;
	}
	else if (myNodeID == 186) {
		nodeID[0] = 185;
		nodeID[1] = 188;
	}
	else if (myNodeID == 187) {
		nodeID[0] = 185;
		nodeID[1] = 188;
	}
	else {
		nodeID[0] = 185;
		nodeID[1] = 186;
		nodeID[2] = 187;
	}
	return nodeID;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	FILE *fp;
	if ((fp = fopen("../topology/topology.dat", "r")) == NULL) {
		printf("cannot open topology.dat!\n");
		exit(0);
	}
	int NodeID1, NodeID2;
	int i;
	char hostname1[32], hostname2[32];
	int cost;
	char localname[32];
	gethostname(localname, sizeof(localname));
	for(i = 0; !feof(fp); ++i) {
		strcpy(hostname1, "vacant line");
		strcpy(hostname2, "vacant line");//to prevent the vacant line
		fscanf(fp, "%s %s %d", hostname1, hostname2, &cost);
		NodeID1 = topology_getNodeIDfromname(hostname1);
		NodeID2 = topology_getNodeIDfromname(hostname2);
		//printf("%s, %s, %d\n", hostname1, hostname2, cost);
		if ((NodeID1 == fromNodeID && NodeID2 == toNodeID) || (NodeID1 == toNodeID && NodeID2 == fromNodeID)) {
			fclose(fp);
			return cost;
		}
	}
	fclose(fp);
	return INFINITE_COST;
}

/*int main() {
	int fromNodeID = 186;
	int toNodeID = 188;
	printf("the cost between %d and %d is: %d\n", fromNodeID, toNodeID, topology_getCost(fromNodeID, toNodeID));
	return 0;
}*/