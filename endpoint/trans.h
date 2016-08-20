/*******************************************************************************

 File Name: trans_ack.h
 Author: Shaopeng Li
 mail: lsp001@mail.ustc.edu.cn
 Created Time: 2016年04月28日 星期四 10时58分59秒

*******************************************************************************/

#ifndef __TRANS_ACK_H__
#define __TRANS_ACK_H__

#include <linux/types.h>
#include <linux/timer.h>
#include <net/ipv6.h>
#include <linux/in6.h>


#include "proto.h"

enum chunk_type {
	CHUNK_SEND,
	CHUNK_RECV,
};

struct content_chunk {
	struct hlist_node node;
	struct timer_list time_out;        //发送req和hsyn超时重发 1000
	struct timer_list time_wait;       //发送ack和needmore后等待 3000
	struct timer_list time_free;       //接收响应包, 超时free 5000
	int timer_count;
	struct in6_addr sip;
	__u8 label[LABEL_SIZE];
	enum chunk_type type;
	size_t count;
	unsigned char* bitmap;
	int COMPLETE_FLAG;
	struct sk_buff* array[];
};

// label -> chunk 映射表
//对接收Chunk的聚合与完整性保证
struct chunk_table {
	struct hlist_head* hash; // -> content_chunk
	spinlock_t lock;
	unsigned int mask;
};

extern struct chunk_table chunk_table;

void chunk_table_init(struct chunk_table* table, const char* name);
void chunk_table_fini(struct chunk_table* table);

//对聚合Chunk表的操作
//return NULL : this label -> chunk no exist
struct content_chunk* chunk_table_search(struct chunk_table* table, const __u8* label, const struct in6_addr* sip);

//insert chunk allocted to chunk_table.
//return 1 :success
//return -1 :fail
int chunk_table_insert(struct chunk_table* table, struct content_chunk* chunk);

//erase the chunk from chunk table. but not free chunk.
//call after push chunk to sock_rbuf.
//return 1 :success
//return -1 :fail
int chunk_table_erase(struct chunk_table* table, struct content_chunk* chunk);

//delete chunk depend on chunk ret_cnt;
int release_chunk(struct content_chunk* chunk);



#endif
