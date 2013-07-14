
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
	int i, j;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int myID = topology_getMyNodeID();
	int N = topology_getNodeNum();//重叠网络中节点总数
	int *nodes = topology_getNodeArray();
    int *nbrs = topology_getNbrArray();

	dv_t* dv = (dv_t*)malloc((1 + num_nbr) * sizeof(dv_t));
	for (i = 0; i < 1 + num_nbr; i++) {
		dv[i].dvEntry = (dv_entry_t*)malloc(N * sizeof(dv_entry_t));
	}

	dv->nodeID = myID;

	for (i = 0; i < N; i++) {
		dv[0].dvEntry[i].nodeID = nodes[i];
		dv[0].dvEntry[i].cost = topology_getCost(myID, dv[0].dvEntry[i].nodeID);
	}
	for (i = 1; i < 1 + num_nbr; i++) {
		dv[i].nodeID = nbrs[i - 1];
		for (j = 0; j < N; j++) {
			dv[i].dvEntry[j].nodeID = nodes[j];
			dv[i].dvEntry[j].cost = INFINITE_COST;
		}
	}
	return dv;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	int i;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	for (i = 0; i < 1 + num_nbr; i++)
		free(dvtable[i].dvEntry);
	free(dvtable);
	return;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i, j;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int N = topology_getNodeNum();//重叠网络中节点总数
	for (i = 0; i < 1 + num_nbr; i++) {
		for (j = 0; j < N; j++) {
			if (dvtable[i].nodeID == fromNodeID && dvtable[i].dvEntry[j].nodeID == toNodeID) {//这两个节点在表中被发现了
				dvtable[i].dvEntry[j].cost = cost;//设置链路代价
				return 1;
			}
		}
	}
	return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i, j;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int N = topology_getNodeNum();//重叠网络中节点总数
	for (i = 0; i < 1 + num_nbr; i++) {
		for (j = 0; j < N; j++) {
			if (dvtable[i].nodeID == fromNodeID && dvtable[i].dvEntry[j].nodeID == toNodeID) {//这两个节点在表中被发现了
				return dvtable[i].dvEntry[j].cost;//返回链路代价
			}
		}
	}
	return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	printf("============dvtable============\n");
	int i, j;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	int N = topology_getNodeNum();//重叠网络中节点总数
	//int myID = topology_getMyNodeID();

	printf("           ");
	for (i = 0; i < N - 1; i++) {
		printf("csnetlab_%d ", i + 1);
	}
	printf("csnetlab_%d\n", N);

	for (i = 0; i < 1 + num_nbr; i++) {
		printf("csnetlab_%d ", (dvtable[i].nodeID - 184));
		for (j = 0; j < N - 1; j++) {
			printf("\t%d", dvtable[i].dvEntry[j].cost);
		}
		printf("\t%d\n", dvtable[i].dvEntry[N - 1].cost);
	}
	return;
}
