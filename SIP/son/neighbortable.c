//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2013年1月

#include "neighbortable.h"
#include "../topology/topology.h"
#include <unistd.h>
#include <stdlib.h>

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
	int i;
	int num_nbr = topology_getNbrNum();//获取邻居节点数
	nbr_entry_t* nt = (nbr_entry_t*)malloc(num_nbr * sizeof(struct neighborentry));//创建邻居表空间
	int* temp = topology_getNbrArray();//获取邻居节点ID数组
	for (i = 0; i < num_nbr; i++) {
		nt[i].nodeID = temp[i];/////////////////
		nt[i].nodeIP = (0x72D4BE << 8) + temp[i];
		nt[i].conn = -1;
	}
	return nt;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
	free(nt);
	return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
	return 0;
}
