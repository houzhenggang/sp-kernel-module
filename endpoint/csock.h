/*******************************************************************************

 File Name: csock.h
 Author: Shaopeng Li
 mail: lsp001@mail.ustc.edu.cn
 Created Time: 2016年04月28日 星期四 08时56分32秒

*******************************************************************************/

#ifndef __CSOCK_H__
#define __CSOCK_H__

#include <linux/types.h>
#include <linux/timer.h>
#include <net/inet_sock.h>

#include "proto.h"
#include "trans.h"
#include "ctable.h"
#include "send.h"

#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/init.h>
#include <linux/in6.h>
#include <linux/ipv6_route.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/skbuff.h>
#include <net/dst.h>

#define LINK_LEN 16
#define IPv6_HEAD_LEN 40
#define EX_IP_LEN 48
#define CHUNK_SIZE 6
#define PAYLOAD_SIZE 1024

#define DEFAULT_LABEL_SET_SIZE 128

struct label_set_node {
	struct list_head node;
	struct csock_bucket* label_node;
	struct csock_list* sock_node;
};

struct chunk_rbuf {
	struct list_head node;

	__u8 label[LABEL_SIZE];
	size_t count;
	struct sk_buff* array[];
//	struct content_chunk* chunk;
};

struct content_sock {
	struct inet_sock inet;

	int FP; // getsockopt/setsockopt ->是否逐条确认?
	
	//待响应请求 set, 请求不重复 -> 指向 ctable 客户端部分
	
	int label_set_size;  //getsockopt/setsockopt -> label_set 哈希表大小?
	
	struct list_head label_set; // -> label_set_node

	//作为服务端时的节点 -> 指向 ctable 服务端部分
	struct csock_server* srv_node;

	struct list_head rbuf; // -> chunk_rbuf 请求/响应 读缓存
};

extern struct content_sock* mysock;
/*
 * 放到rbuf的时候记得调用这个,每个sock都得调用
 */
static inline void sock_def_readable(struct sock* sk)
{
    struct socket_wq *wq;

    rcu_read_lock();
    wq = rcu_dereference(sk->sk_wq);
    if(wq_has_sleeper(wq))
        wake_up_interruptible_sync_poll(&wq->wait, POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND);
    sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN);
    rcu_read_unlock();
}

static inline struct content_sock* csock(struct sock* sk)
{
	return (struct content_sock*)sk;
}

int csock_init(struct sock* sk);
void csock_fini(struct sock* sk, long timeout);

int csock_sendmsg(struct sock* sk, struct msghdr* msg, size_t len);

int csock_recvmsg(struct sock* sk, struct msghdr* msg, size_t len, int noblock,
				  int flags, int *addr_len);

int csock_rcv_skb(struct content_sock* sk, struct sk_buff* skb);

int csock_setsockopt(struct sock* sk, int level, int optname,
					 char __user *optval, unsigned int optlen);

int csock_getsockopt(struct sock* sk, int level, int optname,
					 char __user *optval, int __user *optlen);

//server: called when chunk is complete. and push this chunk to each sock rbuf
int push_chunk_srv_rbuf(struct sock* sk, struct content_chunk* chunk);

//client: called when chunk is complete. and push this chunk to each sock rbuf
int push_chunk_cli_rbuf(struct csock_bucket* bucket, struct content_chunk* chunk);

//when close sock. we should release this sock_rbuf
void release_sock_rbuf(struct sock* sk);



#endif
