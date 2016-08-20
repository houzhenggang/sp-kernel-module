/*******************************************************************************

 File Name: ctable.h
 Author: Shaopeng Li
 Email: lsp001@ustc.edu.cn
 Created Time: 2016年05月01日 星期日 20时52分18秒

*******************************************************************************/

#ifndef __CTABLE_H__
#define __CTABLE_H__

#include <linux/types.h>
#include <net/sock.h>

#include "proto.h"
#include "csock.h"

#define CONTENT_TABLE_SIZE 64*1024 //num of client and ser table.

//客户端 label -> sock 映射表

struct csock_list {
	struct list_head node;
	struct sock* sk;
};

struct csock_bucket {
	struct hlist_node node;
	struct list_head head; // -> csock_list
	__u8 label[LABEL_SIZE];
};

//服务端 filter -> sock 映射表
struct csock_server {
	struct list_head node;
	struct sock* sk;
	int filter_len;  // <= LABEL_SIZE
	__u8 filter[];
};

struct content_table {
	struct hlist_head *cli_hash; // -> csock_bucket
	spinlock_t cli_lock;
	unsigned int mask;

	struct list_head srv_hash; // -> csock_server
	spinlock_t srv_lock;
};

extern struct content_table content_table;

static inline __u32 sn_hash(__u32* label, int len)
{
	int i;
	__u32 hash;
	if (len < 1) 
	{
		return 0;
	}
	hash = label[0];
	for (i = 1; i < len; ++i) 
	{
		hash ^= label[i];
	}
	return hash;
}

//label hash to int range [0  mask-1]
int content_hashfn( __u8* label, unsigned int mask);

//sock->label_set operate
//init this sock->label_set in csock_init function
//fini in csock_fini function
//search this label in sock_label_set
//return 1 : this label exist in
//return 0 : no this label
int sock_label_set_search(struct sock* sk, __u8* label);

//insert label(bucket) and others sk in label to sock_label_set
//not have two similar label. now is bad
//return 1: insert success
//return 0: fail
int sock_label_set_insert(struct sock* sk, struct csock_bucket* bucket, struct csock_list* list);

//erase this label from sk_label_set and label->sock
void sock_label_set_erase(struct sock* sk, __u8* label);

//call insmod content.ko and rmmod content.
//init content table :hlist_head and lock
void content_table_init(struct content_table* table, const char* name);
void content_table_fini(struct content_table* table);


//对客户端 label -> sock 映射操作
//search label in ctable(label->sock)
//return NULL :this label is not exist.
//return bucket : this label is exist, and maybe have sock in or not  
struct csock_bucket* ctable_search(struct content_table* table, __u8* label);

//insert label->sock to table. and call sock_label_set_insert
//return 1: success
//reutrn 0: fail
int ctable_insert(struct content_table* table, __u8* label, struct sock* sk);

//erase sk ,so should del label->sock and sock->label_set
void ctable_erase(struct sock* sk);

//对服务端 filter -> sock 映射操作
struct sock* stable_match(struct content_table* table, __u8* label);
void stable_insert(struct content_table* table, __u8* filter, int filter_len, struct sock* sk);
void stable_erase(struct sock* sk);

#endif
