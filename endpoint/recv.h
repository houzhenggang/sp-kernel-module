/*************************************************************************
	> File Name: recv.h
	> Author: zzfan
	> Mail: zzfan@mail.ustc.edu.cn
	> Created Time: Wed 11 May 2016 06:31:41 AM PDT
 ************************************************************************/
#include "ctable.h"
#include "csock.h"
#include "trans.h"
#include "proto.h"

#include <net/inet_sock.h>

/* 这个文件里的函数返回1表示成功, 0表示失败*/

/*
 * 1.返回ack并且放到content_chunk,设置定时器
 * 2.push chunk to content sock rbuf
 */
int recv_req(struct sk_buff* skb);

/*
 * 1.聚合
 * 2.超时处理
 */
int recv_resp(struct sk_buff* skb);

/*
 * 1.free chunk和chunk中的包
 * 2.设置定时器
 */
int recv_ack(struct sk_buff* skb);

/*
 * 1.找到对应的chunk
 * 2.查看bitmap,满的话回ack,没有的话回needmore
 */
int recv_hsyn(struct sk_buff* skb);

/*
 * 1.根据bitmap重传响应数据包
 */
int recv_needmore(struct sk_buff* skb);

//int update_chunk(struct content_chunk* chunk, struct sk_buff* skb, int sequence);





