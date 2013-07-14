
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
	int i;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int myID = topology_getMyNodeID();
	nbr_cost_entry_t* nct = (nbr_cost_entry_t*)malloc(num_nbr * sizeof(nbr_cost_entry_t));//创建邻居表空间
	int* nbrs = topology_getNbrArray();//获取邻居节点ID数组
	for (i = 0; i < num_nbr; i++) {
		nct[i].nodeID = nbrs[i];/////////////////
		nct[i].cost = topology_getCost(myID, nct[i].nodeID);
	}
	return nct;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	free(nct);
	return;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	int i;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int myID = topology_getMyNodeID();
	for (i = 0; i < num_nbr; i++) {
		if (nct[i].nodeID == nodeID)
			break;
	}
	if (i == num_nbr) {
		return INFINITE_COST;
	}
	return topology_getCost(myID, nct[i].nodeID);
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	printf("============nbrcosttable============\n");
	int i;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int myID = topology_getMyNodeID();
	for (i = 0; i < num_nbr; i++) {
		printf("%d--->%d: %d\n", myID, nct[i].nodeID, nct[i].cost);
	}
	return;
}
