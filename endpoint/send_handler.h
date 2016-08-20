/*************************************************************************
	> File Name: send_handler.h
	> Author: zzfan
	> Mail: zzfan@mail.ustc.edu.cn 
	> Created Time: Wed 11 May 2016 06:33:11 PM PDT
 ************************************************************************/

#ifndef __SEND_HANDLER_H__
#define __SEND_HANDLER_H__

#include "ctable.h"
#include "csock.h"
#include "trans.h"
#include "proto.h"
#include "send.h"

extern struct chunk_table chunk_table;

void req_time_out(unsigned long data);

int req_send_add(struct sk_buff* skb, const char label[], struct in6_addr saddr);

void hsyn_time_out(unsigned long data);

int resp_send_add(struct content_chunk* chunk);

/*
 * ack超时回调函数
 */
void ack_time_wait(unsigned long data);

/*
 * 发送ack之后找到相应的chunk
 */
int ack_send_add(const char label[], struct in6_addr saddr);

/*
 * needmore超时回调函数
 */
void needmore_time_wait(unsigned long data);

/*
 * 发送needmore之后找到相应的chunk
 */
int needmore_send_add(const char label[], struct in6_addr saddr);
#endif
