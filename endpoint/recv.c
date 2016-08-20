#include "recv.h"
#include "trans.h"
#include "send.h"
#include "ctable.h"
#include "csock.h"
#include "proto.h"
#include "send.h"
#include <linux/string.h>
#include <linux/timer.h>

int flag = 0;
//not for resp
__u8 *get_label(struct sk_buff *skb){
    struct content_hdr *ext  = (struct content_hdr *)(skb->data);
    return (__u8 *)ext + sizeof(struct content_hdr);
}

int recv_req(struct sk_buff* skb){
    __u8 *label;
    struct content_chunk *chunk;
    struct sock *sk;
    struct ipv6hdr *iph;

    // TODO checksum
    iph = ipv6_hdr(skb);

    label = get_label(skb);
    //先查看filter与sock的映射表
    sk = stable_match(&content_table, label);
    if(!sk){
         printk("This host does not serve this label!\n");
         return 0;
    }
    printk("request handling, send ack\n");
    send_ack(iph->daddr, iph->saddr, label);

    chunk = chunk_table_search(&chunk_table, label, &iph->saddr);
    if(!chunk){
        chunk = kmalloc(sizeof(struct content_chunk) + sizeof(struct sk_buff *), GFP_ATOMIC);
        memcpy(chunk->label, label, LABEL_SIZE);
        chunk->type = CHUNK_RECV;
        chunk->count = 1;
        chunk->sip = iph->saddr;
        chunk->array[0] = skb;
        chunk_table_insert(&chunk_table, chunk);
    }
    push_chunk_srv_rbuf(sk, chunk);
    return 1;
}

void resp_wait_too_long(unsigned long data){
    //两个响应包之间相隔太久
    struct content_chunk *chunk = (struct content_chunk *)data;
    //从表中删除chunk
	printk("recv::resp_wait_too_long\n");
    chunk_table_erase(&chunk_table, chunk);
    //释放chunk
    release_chunk(chunk);
    return;
}

int update_chunk(struct content_chunk* chunk, struct sk_buff* skb, int sequence){

    int byte, bit;

    //刷新定时器
    mod_timer(&(chunk->time_free), jiffies + msecs_to_jiffies(TIME_FREE));

    byte = sequence / 8;
    bit = sequence % 8;

    //1 代表need, 0 代表已有
    chunk->bitmap[byte] &= ~(0x01 << bit);

    chunk->array[sequence] = skb;
    return 1;
}

int recv_resp(struct sk_buff* skb){
    __u8 *label;
    struct content_response *resp;
    struct content_chunk *chunk;
    struct ipv6hdr *iph;
    int bm_len;
    int i;
    __u8 bits_left;

    // TODO checksum
    resp = (struct content_response *)(skb->data);
    label = resp->label;
    if(ntohs(resp->frag_num) < 1 || ntohs(resp->sequence) >= ntohs(resp->frag_num)){
        printk("fragment information error!\n");
				goto err;
    }
    iph = ipv6_hdr(skb);
    //根据label查找对应的chunk
    chunk = chunk_table_search(&chunk_table, label, &iph->saddr);
    if(!chunk){
        //chunk表为空时, 查找label-sock映射表
        struct csock_bucket *bucket = ctable_search(&content_table, label);
        if(!bucket){
             printk("The label is not requested!\n");
             goto err;
        }

        //bucket不为空但chunk为空, 说明这是第一个响应包, 构造新chunk
        chunk = kmalloc(sizeof(struct content_chunk) + ntohs(resp->frag_num) * sizeof(struct sk_buff *), GFP_ATOMIC);

				if(chunk == NULL) 
					goto err;
		
        //设置定时器, 超时删除chunk	
        setup_timer(&(chunk->time_free), resp_wait_too_long, (unsigned long)chunk);
        mod_timer(&(chunk->time_free), jiffies + msecs_to_jiffies(TIME_FREE));

        memcpy(chunk->label, label, LABEL_SIZE);
	chunk->COMPLETE_FLAG = 0;
        chunk->sip = iph->saddr;
        chunk->type = CHUNK_RECV;
        chunk->count = ntohs(resp->frag_num);
        memset(chunk->array, 0, chunk->count * sizeof(struct sk_buff *));
        //计算bitmap数组长度, 以字节为单位
        bm_len = (chunk->count + 7)/8;
        chunk->bitmap = kmalloc(bm_len, GFP_ATOMIC);
        //bitmap初始化为全1, 代表需要全部的包
        for(i=0;i<bm_len;i++){
            chunk->bitmap[i] = 0xff;
        }
        bits_left = chunk->count % 8;
        //将最后一字节多余的比特置为0
        if(bits_left){
            //01111111右移
            chunk->bitmap[bm_len - 1] = 0x7f >> (7 - bits_left);
        }
        //将新chunk插入chunk_table
        chunk_table_insert(&chunk_table, chunk);
    }
    //将新到来的skb插入到该chunk中, 更新bitmap, 并刷新定时器
		/*
		if(ntohs(resp->sequence) == 1 && flag == 0) 
		{ 
    	printk("recv::recv_hsyn: flag = %d\n", flag);
			flag = 1; 
			return 1;
		}
		*/
    if(1 == chunk->COMPLETE_FLAG) return 1;
    update_chunk(chunk, skb, ntohs(resp->sequence));
    return 1;

err:
	kfree_skb(skb);
	return 0;
}

int recv_ack(struct sk_buff* skb){

    __u8 *label;
    struct content_chunk *chunk = NULL;
    struct ipv6hdr *iph;

    label = get_label(skb);
    iph = ipv6_hdr(skb);
    chunk = chunk_table_search(&chunk_table, label, &iph->saddr);
    if(!chunk){
         printk("Unknown ack!\n");
         return 0;
    }
    chunk->timer_count = 0;
    del_timer(&chunk->time_out);
    chunk_table_erase(&chunk_table, chunk);
    release_chunk(chunk);
    return 1;
}

int recv_hsyn(struct sk_buff* skb){

    __u8 *label;
    struct content_chunk *chunk;
    struct ipv6hdr *iph;
    int i;
    int bm_len;
    struct csock_bucket *bucket = NULL;

    label = get_label(skb);
    iph = ipv6_hdr(skb);
    chunk = chunk_table_search(&chunk_table, label, &iph->saddr);

    if(!chunk){
         printk("Unknown hsyn!\n");
         return 0;
    }
    mod_timer(&(chunk->time_free), jiffies + msecs_to_jiffies(TIME_FREE));

    bm_len = (chunk->count + 7)/8;
    iph = ipv6_hdr(skb);

    //遍历bitmap,
    for(i=0;i<bm_len;i++){
        //有一个元素不为0, 说明缺数据包, 则发送needmore
        if(chunk->bitmap[i]){
            send_needmore(iph->daddr, iph->saddr, label, chunk->bitmap, bm_len);
            return 1;
        }
    }
    printk("recv::recv_hsyn: bitmap[0] = %d\n", (int)(chunk->bitmap[0]));

    //否则发送ack并将chunk交给sock
		flag = 0;
    chunk->COMPLETE_FLAG = 1;
    send_ack(iph->daddr, iph->saddr, label);
		del_timer(&chunk->time_free);
    bucket = ctable_search(&content_table, label);
//	chunk_table_erase(&chunk_table, chunk);	
    push_chunk_cli_rbuf(bucket, chunk);
    return 1;
}

int recv_needmore(struct sk_buff* skb){

    __u8 *label;
    struct content_chunk *chunk;
    struct ipv6hdr *iph;

    //bm_lost 代表丢失包的sequence号
    uint16_t bm_len, bm_lost;

    //one_or_zero是临时变量, 是bitmap中每一bit的值
    __u8 *bitmap, one_or_zero;

    //循环变量
    int i, j;

    label = get_label(skb);
    iph = ipv6_hdr(skb);
    chunk = chunk_table_search(&chunk_table, label, &iph->saddr);
    if(!chunk){
         printk("Unknown needmore!\n");
         return 0;
    }

		//delete time_out and than init timt_out
		del_timer(&chunk->time_out);

    //48是bitmap长度字段相对于协议首部的偏移量, 50是bitmap
    bm_len = ntohs(*((uint16_t *)(skb->data + 48)));
    bitmap = skb->data + 50;
    for(i=0;i<bm_len;i++){
        //bitmap中的一个字节不为0, 说明对应的八个位置出现丢包
        if(bitmap[i]){
            for(j=0;j<8;j++){
                //查看bitmap[i]中第j位是否为1
                one_or_zero = (0x01 << j) & bitmap[i];
                if(one_or_zero){
                    bm_lost = i * 8 + j;
                     send_packet(chunk->array[bm_lost]);
                }
            }
        }
    }

    //chunk->array数组中最后一个元素是hsyn数据包
    send_packet(chunk->array[chunk->count - 1]);

		//init time_out because of hsyn
    init_timer(&chunk->time_out);
    setup_timer(&chunk->time_out, hsyn_time_out, (unsigned long)chunk);
    mod_timer(&chunk->time_out, jiffies + msecs_to_jiffies(REQ_TIME_WAIT));

    return 1;
}
