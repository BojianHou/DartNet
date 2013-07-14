//文件名: common/constants.h

//描述: 这个文件包含常量

//创建日期: 2013年1月

#ifndef SEGHANDLER_H
#define SEGHANDLER_H
#include <pthread.h>
#include "../common/seg.h"

void* seghandler_client(int* src_nodeID, seg_t* seg, long i, long sip_conn);
void* seghandler_server(int* src_nodeID, seg_t* seg, long i, long sip_conn);
void* seghandler(void* arg);

#endif
