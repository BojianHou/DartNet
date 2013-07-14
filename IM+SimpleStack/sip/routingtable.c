
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
	return node % MAX_ROUTINGTABLE_SLOTS;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
	int i;
	int slot_num;
	int* nbr_array = topology_getNbrArray();
	int nbr_num = topology_getNbrNum();
	routingtable_t* routingtable = (routingtable_t*)malloc(sizeof(routingtable_t));
	for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
		routingtable->hash[i] = NULL;
	}
	for (i = 0; i < nbr_num; i++) {
		routingtable_entry_t *new_entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		new_entry->destNodeID = nbr_array[i]; //目标节点ID
		new_entry->nextNodeID = nbr_array[i]; //报文应该转发给的下一跳节点ID
		new_entry->next = NULL;
		slot_num = makehash(nbr_array[i]);//获取槽的位置
		/**插到相应槽的开头**/
		new_entry->next = routingtable->hash[slot_num];
		routingtable->hash[slot_num] = new_entry;
	}
	return routingtable;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	free(routingtable);
	return;
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
	int slot_num;
	routingtable_entry_t *ptr;
	slot_num = makehash(destNodeID);
	ptr = routingtable->hash[slot_num];
	while (ptr != NULL) {//寻找是否存在给定目的节点的路由条目
		if (destNodeID == ptr->destNodeID)
			break;
		ptr = ptr->next;
	}
	if (ptr == NULL) {//说明路由条目不存在，添加一条
		routingtable_entry_t *new_entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		new_entry->destNodeID = destNodeID; //目标节点ID
		new_entry->nextNodeID = nextNodeID; //报文应该转发给的下一跳节点ID
		new_entry->next = NULL;
		/**插到相应槽的开头**/
		new_entry->next = routingtable->hash[slot_num];
		routingtable->hash[slot_num] = new_entry;
	}
	else {
		ptr->nextNodeID = nextNodeID;
	}
	
	return;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
	int slot_num;
	routingtable_entry_t *ptr;
	slot_num = makehash(destNodeID);
	ptr = routingtable->hash[slot_num];
	while (ptr != NULL) {//寻找是否存在给定目的节点的路由条目
		if (destNodeID == ptr->destNodeID)
			break;
		ptr = ptr->next;
	}
	if (ptr == NULL)
		return -1;
	else
		return ptr->nextNodeID;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
	printf("============routingtable============\n");
	int i;
	routingtable_entry_t *ptr;
	printf("DestNode ");
	for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
		if (routingtable->hash[i] != NULL) {
			ptr = routingtable->hash[i];
			while (ptr != NULL) {
				print_csnetlab(ptr->destNodeID);
				printf(" ");
				ptr = ptr->next;
			}
		}
	}
	printf("\nNextNode ");
	for (i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
		if (routingtable->hash[i] != NULL) {
			ptr = routingtable->hash[i];
			while (ptr != NULL) {
				print_csnetlab(ptr->nextNodeID);
				printf(" ");
				ptr = ptr->next;
			}
		}
	}
	printf("\n");
	return;
}

void print_csnetlab(int nodeID) {
	switch(nodeID) {
		case 185:
			printf("csnetlab_1");
			break;
		case 186:
			printf("csnetlab_2");
			break;
		case 187:
			printf("csnetlab_3");
			break;
		case 188:
			printf("csnetlab_4");
			break;
	}
}