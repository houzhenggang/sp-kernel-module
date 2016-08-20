/*************************************************************************
	> File Name: send.h
	> Author: zzfan
	> Mail: zzfan@mail.ustc.edu.cn 
	> Created Time: Wed 11 May 2016 05:35:21 AM PDT
 ************************************************************************/

#ifndef __SEND_H__
#define __SEND_H__

#include "proto.h"
#include "trans.h"
#include "ctable.h"
#include "csock.h"
#include "send_handler.h"

extern struct chunk_table chunk_table;

int send_packet(struct sk_buff* skb);


/*
 * 发送一个请求设置定时器:1.返回了ack,receive取消定时器 2.超时重发
 */
int send_req(struct sock* sk, struct in6_addr saddr, struct in6_addr daddr, const char label[]);

/*
 * 分片发送,发送完了,发送hsyn
 */
int send_resp(struct sock* sk, struct in6_addr saddr, struct in6_addr daddr, const char label[], const char* buff, const unsigned int len);

/*
 * 设置定时器
 */
int send_ack(struct in6_addr saddr, struct in6_addr daddr, const char label[]);

/*
 * 设置定时器
 */
int send_hsyn(struct in6_addr saddr, struct in6_addr daddr, const char label[]);

int send_needmore(struct in6_addr saddr, struct in6_addr daddr, const char label[], unsigned char* bitmap, uint16_t len);

struct sk_buff* create_resp(struct in6_addr saddr, struct in6_addr daddr, const char label[], uint16_t sequence, uint16_t frag_num, const char* buff, const unsigned int offset, const unsigned int skb_data_len);

struct sk_buff* create_hsyn(struct in6_addr saddr, struct in6_addr daddr, const char label[]);

struct sk_buff* create_req(struct in6_addr saddr, struct in6_addr daddr, const char label[]);

struct sk_buff* create_ack(struct in6_addr saddr, struct in6_addr daddr, const char label[]);

struct sk_buff* create_needmore(struct in6_addr saddr, struct in6_addr daddr, const char label[], unsigned char* bitmap, uint16_t len);

struct in6_addr get_saddr(struct sock* sk, struct in6_addr daddr);

#endif

