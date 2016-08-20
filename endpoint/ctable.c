/*******************************************************************************

 File Name: ctable.c
 Author: Shaopeng Li
 mail: lsp001@mail.ustc.edu.cn
 Created Time: 2016年04月27日 星期三 21时37分40秒

*******************************************************************************/

#include <linux/bootmem.h>
#include <net/ipv6.h>

#include "ctable.h"
#include <net/netns/hash.h>


struct content_table content_table;


int content_hashfn(__u8* label, unsigned int mask)
{
	int num = sn_hash((__u32*)label, LABEL_SIZE / 4);
	return num & (mask-1);
}

static void free_csock_bucket(struct csock_bucket* bucket)
{
	struct csock_list *list = NULL, *temp = NULL;
	if(!list_empty(&bucket->head))
	{
		//printk("ctable::free_csock_bucket: list have sock\n");
		list_for_each_entry_safe(list, temp, &bucket->head, node)
		{
			list_del(&list->node);
			kfree(list);
			//printk("ctable::free_csock_bucket: free sock\n");
		}
	}
	hlist_del(&bucket->node);
	kfree(bucket);
}

static void free_content_table(struct content_table* table)
{
	unsigned int i;
	struct hlist_head* cli_hash = NULL;
	struct csock_bucket* bucket = NULL;
	struct hlist_node* hnode = NULL;
	struct csock_server *server = NULL, *temp = NULL;
	printk("ctable: clean lebel->sock\n");
	for(i = 0; i < CONTENT_TABLE_SIZE; i++)
	{
		cli_hash = &content_table.cli_hash[i];
		if(!hlist_empty(cli_hash)) {
			printk("ctable: bucket have lebel\n");
			hlist_for_each_entry_safe(bucket, hnode, cli_hash, node)
			{
				free_csock_bucket(bucket);
			}
		}
	}
	if(!list_empty(&content_table.srv_hash))
	{
		list_for_each_entry_safe(server, temp, &content_table.srv_hash, node)
		{
			list_del(&server->node);
			kfree(server);
		}
	}
}

//init content_table
//called when insmod content.ko
void content_table_init(struct content_table* table, const char* name)
{
	unsigned int i;
  content_table.cli_hash = kmalloc(sizeof(struct hlist_head)*CONTENT_TABLE_SIZE, GFP_ATOMIC);
	if(content_table.cli_hash == NULL)
	{
		return;
	}
  content_table.mask = CONTENT_TABLE_SIZE;
	spin_lock_init(&table->cli_lock);

	for(i = 0; i < CONTENT_TABLE_SIZE; i++)
	{
		INIT_HLIST_HEAD(&content_table.cli_hash[i]);
	}

	//init srv_hash csock_server
	INIT_LIST_HEAD(&table->srv_hash);
	spin_lock_init(&table->srv_lock);
	printk("ctable: finished init content table\n");
}

//free content_table
//called when rmmod content
void content_table_fini(struct content_table* table)
{
	free_content_table(table);
	kfree(content_table.cli_hash);
	printk("ctable: free content table\n");
}

//search this label in sock_label_set
//return 1 : this label exist in
//return 0 : no this label
int sock_label_set_search(struct sock* sk, __u8* label)
{
	struct content_sock* ctsock = csock(sk);
	if(!list_empty(&ctsock->label_set))
	{
		struct label_set_node* label_set_node = NULL;
		list_for_each_entry(label_set_node, &ctsock->label_set, node)
		{
			if(memcmp(label, label_set_node->label_node->label, LABEL_SIZE) ==0)
			{
				printk("ctable: sock->label_set have sock->label\n");
				return 1;
			}
		}
	}
	return 0;
}

//insert label(bucket) and others sk in label to sock_label_set
//not have two similar label. now is bad
//return 1: insert success
//return 0: fail
int sock_label_set_insert(struct sock* sk, struct csock_bucket* bucket, struct csock_list* list)
{
	//struct content_sock* ctsock = csock(sk);
	struct label_set_node* label_set_node = NULL;
	if(!sock_label_set_search(sk, bucket->label))
	{
		label_set_node = (struct label_set_node*)kmalloc(sizeof(struct label_set_node), GFP_ATOMIC);
		if(label_set_node == NULL) return -1;
		//init label_set_node
		INIT_LIST_HEAD(&label_set_node->node);
		label_set_node->label_node = bucket;
		label_set_node->sock_node = list;
		//insert
		list_add(&label_set_node->node, &csock(sk)->label_set);
		printk("ctable: insert sock->label\n");
		return 1;
	}
	return 0;
}

//erase this label from sk_label_set, and sock in label->sock, if label have no sock , so rm label

void sock_label_set_erase(struct sock* sk, __u8* label)
{
	struct content_sock* ctsock = csock(sk);
	struct list_head* label_set = &ctsock->label_set;
	//struct list_head* hnode = NULL;
	struct label_set_node *label_set_node = NULL, *hnode = NULL;
	if(!list_empty(label_set))
	{
		list_for_each_entry_safe(label_set_node, hnode, label_set, node)
		{
			if(memcmp(label, label_set_node->label_node->label, LABEL_SIZE) == 0)
			{
				//free label_set label
				if(NULL != label_set_node)
				{
					list_del(&label_set_node->node);
					kfree(label_set_node);
					//label_set_node = NULL;// kaolv
					printk("ctable: remove sock_label_set's label\n");
				}

				//free ctable label->sock
				if(NULL != label_set_node->sock_node)
				{
					list_del(&label_set_node->sock_node->node);
					kfree(label_set_node->sock_node);
					//label_set_node->sock_node = NULL; //kaolv
					printk("ctable: remove label->sock sock\n");
				}
				if(NULL != label_set_node->label_node )
				{
					if(list_empty(&label_set_node->label_node->head))
					{
						hlist_del(&label_set_node->label_node->node);
						kfree(label_set_node->label_node);
						//label_set_node->sock_node = NULL; //kaolv
						printk("ctable: remove lable, because lable have no sock\n");
					}
				}
			}
		}
	}
	printk("ctable: remove sock_label_set's label, and remove ctable's label->sock\n");
}

//search label in ctable(label->sock)
//return NULL :this label is not exist.
//return bucket : this label is exist, and maybe have sock in or not
struct csock_bucket* ctable_search(struct content_table* table, __u8* label)
{
	int index = content_hashfn(label, table->mask);
	struct hlist_head* cli_hash = &content_table.cli_hash[index];
	struct csock_bucket* bucket = NULL;

	if(!hlist_empty(cli_hash))
	{ //empty return 1
		hlist_for_each_entry(bucket, cli_hash, node)
		{
			if(memcmp(label, bucket->label, LABEL_SIZE) == 0)
			{
				printk("ctable: label->sock table have label\n");
				return bucket;
			}
		}
	}
	return NULL;
}

//call after sendmsg.
//insert label->sock to table. and call sock_label_set_insert
//return 1: success
//reutrn 0: fail
int ctable_insert(struct content_table* table, __u8* label, struct sock* sk)
{
	struct hlist_head* cli_hash = NULL;
	struct csock_bucket* bucket = NULL;
	struct csock_list* list = NULL;

	int index = content_hashfn(label, table->mask);
	cli_hash = &content_table.cli_hash[index];
	//find lable in content_table
	bucket = ctable_search(table, label);

	if(bucket == NULL)
	{
		bucket = (struct csock_bucket*)kmalloc(sizeof(struct csock_bucket), GFP_ATOMIC);
		if(bucket == NULL)
		{
			return -1;
		}
		// init bucket node
		memcpy(bucket->label, label, LABEL_SIZE);
		INIT_LIST_HEAD(&bucket->head);
		// init sock list node
		list = (struct csock_list*)kmalloc(sizeof(struct csock_list), GFP_ATOMIC);
		if(list == NULL) {
			return -1;
		}
		list->sk = sk;
		list_add(&list->node, &bucket->head);
		//insert bucket to content_table;
		spin_lock(&table->cli_lock);
		hlist_add_head(&bucket->node, cli_hash);
		spin_unlock(&table->cli_lock);
		printk("ctable: insert label->sock because label = N, sock = N\n");
		if(sock_label_set_insert(sk, bucket, list) != 1)
		{
			printk("ctable: error insert sock_label_set\n");
			return 0;
		}
		return 1;
	}
	else
	{
		// find label in sock_label_set. return 1: have this label
		if(!sock_label_set_search(sk, label))
		{
			list = (struct csock_list*)kmalloc(sizeof(struct csock_list), GFP_ATOMIC);
			if(list == NULL)
			{
				return -1;
			}
			list->sk = sk;
			list_add(&list->node, &bucket->head);
			printk("ctable: insert the label->sock, because label = Y, sock = N\n");
			if(sock_label_set_insert(sk, bucket, list) != 1)
			{
				printk("ctable: error insert sock_label_set\n");
				return 0;
			}
		}
	}
	return 0;
}


void ctable_erase(struct sock* sk)
{
	struct content_sock* ctsock = csock(sk);
	struct list_head* label_set = &ctsock->label_set;
	struct label_set_node *label_set_node = NULL, *hnode = NULL;

	if(!list_empty(label_set))
	{
		printk("csock:label_set is not empty\n");
		list_for_each_entry_safe(label_set_node, hnode, label_set, node)
		{
			list_del(&label_set_node->node);
			kfree(label_set_node);
			//label_set_node = NULL;
			printk("ctable: remove sock_label_set's label\n");

			//free ctable label->sock
			if(NULL != label_set_node->sock_node)
			{
				list_del(&label_set_node->sock_node->node);
				kfree(label_set_node->sock_node);
				//label_set_node->sock_node = NULL; //kaolv
				printk("ctable: remove label->sock sock\n");
			}
			if(NULL != label_set_node->label_node )
			{
				if(list_empty(&label_set_node->label_node->head))
				{
					hlist_del(&label_set_node->label_node->node);
					kfree(label_set_node->label_node);
					//label_set_node->sock_node = NULL; //kaolv
					printk("ctable: remove lable, because lable have no sock\n");
				}
			}
		}
	}
}

//对服务端 filter -> sock 映射操作
struct sock* stable_match(struct content_table* table, __u8* label)
{
	if(!list_empty(&table->srv_hash))
	{
		struct csock_server* server = NULL;
		list_for_each_entry(server, &table->srv_hash, node)
		{
			if(memcmp(label, server->filter, server->filter_len) == 0)
			{
				printk("stable: filter exist\n");
				return server->sk;
			}
		}
	}
	return NULL;
}

void stable_insert(struct content_table* table, __u8* filter, int filter_len, struct sock* sk)
{
	struct csock_server* server = NULL;
	if(stable_match(table, filter) == NULL)
	{
		server = (struct csock_server*)kmalloc(sizeof(struct csock_server), GFP_ATOMIC);
		if(NULL == server) return;
		INIT_LIST_HEAD(&server->node);
		server->sk = sk;
		server->filter_len = filter_len;
		memcpy(server->filter, filter, filter_len);
		list_add(&server->node, &table->srv_hash);
		printk("stable: insert filter->sock\n");

		// insert this node to sock->src_node
		csock(sk)->srv_node = server;
		printk("stable: insert sock's src_node\n");
	}
	else
	{
		printk("stable: filter exist, not insert\n");
	}
}

void stable_erase(struct sock* sk)
{
	struct csock_server* srv_node = csock(sk)->srv_node;
	if(NULL != srv_node)
	{
		list_del(&srv_node->node);
		kfree(srv_node);
		srv_node = NULL;
		printk("stable: free sock->srv_node, and filter->sock\n");
	}
}

