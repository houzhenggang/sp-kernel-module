/*******************************************************************************

  File Name: trans_ack.c
Author: Shaopeng Li
mail: lsp001@mail.ustc.edu.cn
Created Time: 2016年04月28日 星期四 10时59分01秒

 *******************************************************************************/

#include "trans.h"
#include "csock.h"


struct chunk_table chunk_table;

static int label_IP_hash(const __u8* label, const struct in6_addr sip, int mask)
{
		int i;
		__u32 hash = label[0];
		for(i = 1; i < LABEL_SIZE; i++)
		{
				hash ^= label[i];
		}
		for(i = 0; i < 16; i++)
		{
				hash ^= sip.s6_addr[i];
		}
		return (hash % mask);
}

static void free_chunk_table(struct chunk_table* table)
{
		unsigned int i, ret;
		struct hlist_head* hash = NULL;
		struct content_chunk* chunk = NULL;
		struct hlist_node* hnode = NULL;
		for(i =0; i < CONTENT_TABLE_SIZE; i++)
		{
				hash = &table->hash[i];
				if(!hlist_empty(hash))
				{
						hlist_for_each_entry_safe(chunk, hnode, hash, node)
						{
								for(ret = 0; ret < chunk->count; ret++)
								{
										if(NULL != chunk->array[ret])
										{
												kfree_skb(chunk->array[ret]);
												chunk->array[ret] = NULL;
										}
								}
								//del_timer(&chunk->time_out);
								hlist_del(&chunk->node);
								kfree(chunk);
						}
				}
		}
}

//call when insmod content.ko
void chunk_table_init(struct chunk_table* table, const char* name)
{
		unsigned int i;
		table->hash = kmalloc(sizeof(struct hlist_head)*CONTENT_TABLE_SIZE, GFP_ATOMIC);
		if(table->hash == NULL)
		{
				return;
		}
		table->mask = CONTENT_TABLE_SIZE;
		spin_lock_init(&table->lock);

		for(i = 0; i < CONTENT_TABLE_SIZE; i++)
		{
				INIT_HLIST_HEAD(&table->hash[i]);
		}
		printk("trans::chunk_table_init: finished init chunk_table\n");
}

//call when rmmod content
void chunk_table_fini(struct chunk_table* table)
{
		free_chunk_table(table);
		kfree(table->hash);
		printk("trans::chunk_table_fini:free content table\n");
}

//return NULL : this label -> chunk no exist
struct content_chunk* chunk_table_search(struct chunk_table* table, const __u8* label, const struct in6_addr* sip)
{
		int index = label_IP_hash(label, *sip, table->mask);
		struct hlist_head* hash = &table->hash[index];
		struct content_chunk* chunk = NULL;

		//printk("trans::chunk_table_search: hash index%d\n", index);
		if(!hlist_empty(hash))
		{ //empty return 1
				hlist_for_each_entry(chunk, hash, node)
				{
						if((memcmp(label, chunk->label, LABEL_SIZE) == 0) &&
										(memcmp(sip, &chunk->sip, sizeof(struct in6_addr)) == 0))
						{
								//printk("trans::chunk_table_search: label->chunk table have label->chunk\n");
								return chunk;
						}
				}
		}
		return NULL;
}

//insert chunk allocted to chunk_table.
//return 1 :success
//return -1 :fail
int chunk_table_insert(struct chunk_table* table, struct content_chunk* chunk)
{
		if(chunk == NULL)
		{
				printk(KERN_ERR "chunk is NULL, you should kmalloc chunk, then insert\n");
				return -1;
		}
		else
		{
				int index = label_IP_hash(chunk->label, chunk->sip, table->mask);
				struct hlist_head* hash = &table->hash[index];
				//printk("trans::chunk_table_insert: chunk_table_insert: hash index%d\n", index);
				spin_lock(&table->lock);
				hlist_add_head(&chunk->node, hash);
				spin_unlock(&table->lock);
				printk("trans::chunk_table_insert: insert label->chunk to chunk_table\n");
				return 1;
		}
}

//erase the chunk from chunk table. but not free chunk.
//call after push chunk to sock_rbuf.
//return 1 :success
//return -1 :fail
int chunk_table_erase(struct chunk_table* table, struct content_chunk* chunk)
{
		struct content_chunk* cret;
		if(chunk == NULL)
		{
				printk("trans: no label->chunk\n");
				return -1;
		}
		else
		{
				cret = chunk_table_search(&chunk_table, chunk->label, &chunk->sip);
				if(NULL != cret)
				{
						spin_lock(&table->lock);
						//hlist_del(&chunk->node);
						hlist_del(&cret->node);
						spin_unlock(&table->lock);
						//not free chunk, because this chunk pointer in sock_rbuff
						printk("trans::chunk_table_erase: free this label chunk of chunk_table\n");
						return 1;
				}
				return -1;
		}
}

//WARNING: This function does not process timers, delete them before calling.
//delete chunk ,
//return: 1 -> free chunk success
//return: 0 -> not free chunk ,ret_cnt not 0
//return: -1 -> error
int release_chunk(struct content_chunk* chunk)
{
	int ret;

	if(NULL == chunk) return -1;

	for(ret = 0; ret < chunk->count; ret++)
	{
		if(NULL != chunk->array[ret])
		{
			kfree_skb(chunk->array[ret]);
		}
	}
	if(!chunk->bitmap)
	{
		kfree(chunk->bitmap);
	}
	kfree(chunk);
	chunk = NULL;
	return 1;
}


