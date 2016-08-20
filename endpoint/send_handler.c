/*************************************************************************
	> File Name: send_handler.h
	> Author: zzfan
	> Mail: zzfan@mail.ustc.edu.cn 
	> Created Time: Wed 11 May 2016 06:33:11 PM PDT
 ************************************************************************/

#include "send_handler.h"

/*
 * 这里不删除请求包,返回ack时删除
 */
void req_time_out(unsigned long data)
{
    struct content_chunk* chunk = (struct content_chunk*)data;
    if(NULL == chunk->array[0]){
        printk("SKB is NULL\n");
        return;
    }

    //计时器计数+1
    chunk->timer_count += 1;

    //再次发送hsyn包,并且重新设定定时器
    if(chunk->timer_count < 4){
        send_packet(chunk->array[0]);
        mod_timer(&chunk->time_out, jiffies + msecs_to_jiffies(REQ_TIME_WAIT));
    }
    else{
        //删除chunk和定时器,这里把计数设为0
        //在测试的时候不要被这个迷惑了
        chunk->timer_count = 0;
        del_timer(&chunk->time_out);
        chunk_table_erase(&chunk_table, chunk);
        release_chunk(chunk);
    }
}

/*
 * 保存请求包,收到ack的时候删除
 */
int req_send_add(struct sk_buff* skb, const char label[], struct in6_addr daddr)
{
//    int ret = 1;
	struct content_chunk* chunk = NULL;
	chunk = kmalloc(sizeof(struct content_chunk) + sizeof(struct sk_buff*), GFP_ATOMIC);

	//构造一个req_chunk
	chunk->sip = daddr;
	chunk->timer_count = 0;
	chunk->count = 1;
	memcpy(chunk->label, label, LABEL_SIZE);
	chunk->array[0] = skb;
	chunk->type = CHUNK_SEND;
	chunk->bitmap = NULL;

    //设置定时器
	init_timer(&chunk->time_out);
	setup_timer(&chunk->time_out, req_time_out, (unsigned long)chunk);
	mod_timer(&chunk->time_out, jiffies + msecs_to_jiffies(REQ_TIME_WAIT));

	chunk_table_insert(&chunk_table, chunk);
	return 0;
}

/*
 * 这个就是hsyn的time_out_callback
 */
void hsyn_time_out(unsigned long data)
{
    struct content_chunk* chunk = (struct content_chunk*)data;
    
    //计时器计数+1
    chunk->timer_count += 1;

    //再次发送hsyn包,并且重新设定定时器
    if(chunk->timer_count < 4){
        send_packet(chunk->array[chunk->count - 1]);
        mod_timer(&chunk->time_out, jiffies + msecs_to_jiffies(REQ_TIME_WAIT));
    }
    else{
        //删除chunk和定时器,这里把计数设为0
        //在测试的时候不要被这个迷惑了
        chunk->timer_count = 0;
        del_timer(&chunk->time_out);
        chunk_table_erase(&chunk_table, chunk);
        release_chunk(chunk);
    }
}

/*
 * 发送完response包后发送hsyn
 * 0: succees
 * -1; fail
 */
int resp_send_add(struct content_chunk* chunk)
{
    //设置定时器
    int ret = 0;
    init_timer(&chunk->time_out);
    setup_timer(&chunk->time_out, hsyn_time_out, (unsigned long)chunk);
    mod_timer(&chunk->time_out, jiffies + msecs_to_jiffies(REQ_TIME_WAIT));

    ret = chunk_table_insert(&chunk_table, chunk);
    if(ret != 0)
        return -1;

    //成功返回0
    return 0;
}

/*
 * delete chunk
 * 收到request返回ack能找到chunk是因为收到request后会创建
 */
void ack_time_wait(unsigned long data)
{
    struct content_chunk* chunk = (struct content_chunk*)data;

    del_timer(&chunk->time_wait);
    chunk_table_erase(&chunk_table, chunk);
		release_chunk(chunk);
}

/*
 * -1: fail to find chunk
 *  0: succees
 */
int ack_send_add(const char label[], struct in6_addr saddr)
{
    struct content_chunk* chunk = chunk_table_search(&chunk_table, label, &saddr);
    if(NULL == chunk)
        return -1;

    init_timer(&chunk->time_wait);
    setup_timer(&chunk->time_wait, ack_time_wait, (unsigned long)chunk);
    mod_timer(&chunk->time_wait, jiffies + msecs_to_jiffies(ACK_TIME_WAIT));

    return 0;
}


/*
 * delete chunk
 */
void needmore_time_wait(unsigned long data)
{
    struct content_chunk* chunk = (struct content_chunk*)data;

    del_timer(&chunk->time_wait);
    chunk_table_erase(&chunk_table, chunk);
}

/*
 * -1: fail to find chunk
 *  0: succees
 */
int needmore_send_add(const char label[], struct in6_addr daddr)
{
    struct content_chunk* chunk = chunk_table_search(&chunk_table, label, &daddr);
    if(NULL == chunk)
        return -1;
/*
    init_timer(&chunk->time_wait);
    setup_timer(&chunk->time_wait, needmore_time_wait, (unsigned long)chunk);
    mod_timer(&chunk->time_wait, jiffies + msecs_to_jiffies(ACK_TIME_WAIT));
*/
    return 0;
}

