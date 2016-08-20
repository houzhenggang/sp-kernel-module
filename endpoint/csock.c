/*******************************************************************************

 File Name: csock.c
 Author: Shaopeng Li
 Email: lsp001@ustc.edu.cn
 Created Time: 2016年05月01日 星期日 20时52分28秒

*******************************************************************************/

#include "csock.h"

int csock_init(struct sock* sk)
{
    //init content_sock
    struct content_sock* ctsock = csock(sk);
    ctsock->FP = 1;
    ctsock->label_set_size = DEFAULT_LABEL_SET_SIZE;
    INIT_LIST_HEAD(&ctsock->label_set);
    ctsock->srv_node = NULL;
    INIT_LIST_HEAD(&ctsock->rbuf);
    inet_sk(sk)->bind_address_no_port = 1;
    return 0;
}

void csock_fini(struct sock* sk, long timeout)
{
    ctable_erase(sk);
    stable_erase(sk);
		release_sock_rbuf(sk);
    sk_common_release(sk);
    printk(KERN_DEBUG "csock::csock_fini: close sock\n");
}

int csock_sendmsg(struct sock* sk, struct msghdr* msg, size_t len)
{
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
    int ret = 0;
    if (!cmsg) {
        return -EOPNOTSUPP;
    }

    switch(cmsg->cmsg_type) {
    case CONTENT_REQ:
        {
            struct in6_addr saddr, daddr = in6addr_any;

            unsigned char label[LABEL_SIZE];
            unsigned char buffer[CHUNK_SIZE];

//            mysock = (struct content_sock*)sk;
            if (msg->msg_name) {
                daddr = *(struct in6_addr*)msg->msg_name;
            } else {
                printk(KERN_ERR "csock::csock_sendmsg: daddr is null\n");
                return 0;
            }
            saddr = get_saddr(sk, daddr);
            memcpy(label, LABEL((*msg).msg_control), LABEL_SIZE);
            memset(buffer, 0, CHUNK_SIZE);

     	    	ctable_insert(&content_table, label, sk);

						printk("csock::csock_sendmsg: send request\n");
            send_req(sk, saddr, daddr, label);
            //send_ack(saddr, daddr, label); //发送ack测试
            //unsigned char* bitmap = 0; //发送needmore测试
            //uint16_t len = 1;
            //send_needmore(saddr, daddr, label, bitmap, len);
            
        }; break;
    case CONTENT_RESP:
        {
            struct in6_addr saddr, daddr = in6addr_any;

            unsigned char label[LABEL_SIZE];
            //char *buffer;
            unsigned int buff_len;

            //mysock = (struct content_sock*)sk;
            if (msg->msg_name) {
                daddr = *(struct in6_addr*)msg->msg_name;
            } else {
                printk(KERN_ERR "csock::csock_sendmsg: daddr is null\n");
                return 0;
            }
            saddr = get_saddr(sk, daddr);
            memcpy(label, LABEL((*msg).msg_control), LABEL_SIZE);
						//copy_from_user(label, LABEL((*msg).msg_control), LABEL_SIZE);

            buff_len = ((msg->msg_iter).iov)->iov_len;
            //buffer = kmalloc(buff_len+1, GFP_ATOMIC);
            printk("csock::csock_sendmsg: data len: %d\n", buff_len);
            //memset(buffer, 0, 1);
            //memcpy(buffer, ((msg->msg_iter).iov)->iov_base, buff_len);
			//copy_from_user(buffer, ((msg->msg_iter).iov)->iov_base, buff_len);

            printk("csock::csock_sendmsg: send response\n");
            send_resp(sk, saddr, daddr, label, ((msg->msg_iter).iov)->iov_base, buff_len);

        }; break;
    default:
        ret = -EOPNOTSUPP;
    }
	return ret;
}

/*
 * FIFO 读取rbuf的chunk。一次读一个chunk，读完并释放此chunk
 * 0:读取成功
 * > 0 
 */
///*
int decl_rbuf_to_msg(struct msghdr* msg, struct content_sock* csk, int *addr_len)
{
    unsigned int seq =0, offset = 0, len =0;
    struct chunk_rbuf *entry = NULL;
    struct sk_buff *skb = NULL;
		struct content_hdr* content_hdr = NULL;
		struct ipv6hdr *ipv6hdr = NULL;
		struct content_label* req_ctl = NULL;
		u_char* data = NULL;

		//memset(msg->msg_iter.iov->iov_base, 0, msg->msg_iter.iov->iov_len);
		if(!list_empty(&csk->rbuf))
		{
			entry = list_entry(csk->rbuf.next, struct chunk_rbuf, node);
			if(NULL == entry) return -1;
            printk("csock::decl_rbuf_to_msg: chunk->count: %lu\n", entry->count);
			while(seq < entry->count)
			{
				skb = entry->array[seq];
				content_hdr = (struct content_hdr*)(skb->data);
				if(seq == 0)
				{
					ipv6hdr = ipv6_hdr(skb);
					//msg_name and msg_namelen
					{
						*((struct in6_addr *)(msg->msg_name)) = ipv6hdr->saddr;
                        /*
                        printk("myaddr: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
                   ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[0]), ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[1]),
                   ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[2]), ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[3]),
                   ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[4]), ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[5]),
                   ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[6]), ((*((struct in6_addr*)(msg->msg_name))).in6_u.u6_addr16[7]));
                 */ 
						msg->msg_namelen = sizeof(struct in6_addr);
                        *addr_len = sizeof(struct sockaddr_in6);
					}	

					//msg_control, msg_controllen
					{
						req_ctl = (struct content_label*)(msg->msg_control);
						req_ctl->hdr.cmsg_len = MSGCTL_SIZE; 
						req_ctl->hdr.cmsg_level = IPPROTO_CONTENT;
						req_ctl->hdr.cmsg_type = content_hdr->type;
						memcpy(req_ctl->label, entry->label, LABEL_SIZE);
						//msg->msg_controllen = MSGCTL_SIZE;
					}
					if(content_hdr->type == CONTENT_TYPE_REQ)
					{
						printk("csock::decl_rbuf_to_msg: this is request rbuf->msg\n");
						kfree_skb(entry->array[seq]);
						list_del(&entry->node);
						kfree(entry);
						return 0;
					}
				}

				//msg_iov, msg_iovlen
				//one skb data payload len
				if(offset > msg->msg_iter.iov->iov_len)
				{
					printk("csock::decl_rbuf_to_msg: msg.iov.len is not enough\n");
					return -1;
				}

				len = ntohs(ipv6hdr->payload_len) - sizeof(struct content_response);
				if(len <= 0) return -1;

				data = (u_char*)(skb->data + 56);
				memcpy((msg->msg_iter.iov->iov_base) + offset, data, len);

				kfree_skb(entry->array[seq]);
				//printk("csock::+++++ %d\n", skb_shinfo(skb)->dataref);
				seq++;
				offset += len;
			}
			list_del(&entry->node);
			kfree(entry);
			return 0;
		}
    return 1;

}

static int receiver_wake_function(wait_queue_t *wait, unsigned int mode, int sync, void *key)
{
    unsigned long bits = (unsigned long)key;
    if(bits && !(bits & (POLLIN | POLLERR)))
    return 0;
    return autoremove_wake_function(wait, mode, sync, key);
}

int wait_for_rbuf(struct sock *sk, int *err, long *timeo_p)
{
    int error;
    struct content_sock* csk = (struct content_sock*)sk; 

    //加入wait队列
    DEFINE_WAIT_FUNC(wait, receiver_wake_function);

    prepare_to_wait_exclusive(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

    printk("csock::wait_for_rbuf:: enter wait\n");
    // Socket errors? 
    error = sock_error(sk);
    if (error)
        goto out_err;

    //check rbuf is null
    if (!list_empty(&csk->rbuf))
        goto out;

    // Socket shut down? 
    if (sk->sk_shutdown & RCV_SHUTDOWN)
        goto out_noerr;

    // 检查是否有pending的signal，保证阻塞时，进程可以被signal唤醒
    // handle signals 
    if (signal_pending(current))
        goto interrupted;

    error = 0;
    // sleep本进程，直至满足唤醒条件或者被信号唤醒——因为前面设置了TASK_INTERRUPTIBLE
    *timeo_p = schedule_timeout(*timeo_p);
out:
    // wait队列的清理工作  
		printk("csock::wait_for_rbuf::Finish_wait\n");
    finish_wait(sk_sleep(sk), &wait);
   
    return error;

interrupted:
    printk("csock::wait_for_rbuf::Enter interrupted\n");
    error = sock_intr_errno(*timeo_p);
out_err:
    *err = error;
    goto out;
out_noerr:
    *err = 0;
    error = 1;
    goto out;
}

int csock_recvmsg(struct sock* sk, struct msghdr* msg, size_t len, int noblock,
                  int flags, int *addr_len)
{
    struct content_sock *csk = (struct content_sock*)sk;
    long timeo;
    int err;
    int res = 0;

    int error = sock_error(sk);
    if(error)
        goto no_rbuf; //goto no_rbuf

//    timeo = sock_rcvtimeo(sk, (flags | (noblock ? MSG_DONTWAIT : 0)) & MSG_DONTWAIT);
    timeo = sock_rcvtimeo(sk, (flags | 0) & MSG_DONTWAIT);
    printk("csock::csock_recvmsg: Set timeo\n");

    if(!list_empty(&csk->rbuf))
        res = decl_rbuf_to_msg(msg, csk, addr_len);
    else{
        do{
            if(!list_empty(&csk->rbuf)){
                decl_rbuf_to_msg(msg, csk, addr_len);
                break;
            }
            printk("csock::csock_recvmsg: Wait loo\n");
            if(!timeo)
                goto no_rbuf;
        }while(!wait_for_rbuf(sk, &err, &timeo)); 
    }

    printk("csock::csock_recvmsg: Exit wait loo\n");
    if(res < 0) return -1;
    return 0;

no_rbuf: 
    return -1;
}

int csock_setsockopt(struct sock* sk, int level, int optname,
					 char __user *optval, unsigned int optlen)
{
    char *label = kmalloc(optlen*sizeof(char), GFP_ATOMIC);
    printk("csock:: csock_setsockopt::%d\n", optlen);
    copy_from_user(label, optval, optlen);
    printk("csock:: csock_setsockopt::%s\n", label);

    stable_insert(&content_table, label, optlen, sk);

	return 0;
}

int csock_getsockopt(struct sock* sk, int level, int optname,
					 char __user *optval, int __user *optlen)
{

	return 0;
}

//server: called when chunk is complete. and push this chunk to each sock rbuf
int push_chunk_srv_rbuf(struct sock* sk, struct content_chunk* chunk)
{
		int seq = 0;
    struct content_sock* csk = NULL;
    struct chunk_rbuf* rbuf = NULL;

    if(NULL == sk || NULL == chunk) return -1;
    csk = csock(sk);
		rbuf = (struct chunk_rbuf*)kmalloc(sizeof(struct chunk_rbuf) + chunk->count * sizeof(struct sk_buff*), GFP_ATOMIC);
    if(NULL == rbuf) return -1;
    INIT_LIST_HEAD(&rbuf->node);
    //rbuf->chunk = chunk;
		rbuf->count = chunk->count;
	  memcpy(rbuf->label, chunk->label, LABEL_SIZE);
	  for(seq = 0; seq < chunk->count; seq++){
			rbuf->array[seq] = skb_clone(chunk->array[seq], GFP_ATOMIC);
	  }
    list_add_tail(&rbuf->node, &csk->rbuf);

    sock_def_readable(sk);

    //remove content sock->srv_node and free filter->sock
    //stable_erase(sk);
    return 1;
}

//client: called when chunk is complete. and push this chunk to each sock rbuf
int push_chunk_cli_rbuf(struct csock_bucket* bucket, struct content_chunk* chunk)
{
	int seq = 0;
   struct csock_list *list = NULL, *temp = NULL;

	if(NULL == bucket || NULL == chunk) return -1;
	list_for_each_entry_safe(list, temp, &bucket->head, node)
	{
		struct content_sock *csk = csock(list->sk);
		struct chunk_rbuf* rbuf = (struct chunk_rbuf*)kmalloc(sizeof(struct chunk_rbuf) + chunk->count * sizeof(struct sk_buff*), GFP_ATOMIC);
		if(NULL == rbuf) return -1;
		INIT_LIST_HEAD(&rbuf->node);
			//rbuf->chunk = chunk;
			rbuf->count = chunk->count;
			memcpy(rbuf->label, chunk->label, LABEL_SIZE);
			for(seq = 0; seq < chunk->count; seq++)
			{
				rbuf->array[seq] = skb_clone(chunk->array[seq], GFP_ATOMIC);
			}
			list_add_tail(&rbuf->node, &csk->rbuf);

			sock_def_readable(list->sk);

			//remove label from sock->label_set and label->sock
			sock_label_set_erase(list->sk, chunk->label);   
		}
    printk("csock::push_chunk_cli_rbuf: chunk is completed and push_chunk_sockrbuff\n");
    return 1;
}

//when close sock. we should release this sock_rbuf
void release_sock_rbuf(struct sock* sk)
{
		int seq;
    struct chunk_rbuf *rbuf = NULL, *temp = NULL;
    struct content_sock* ctsock = csock(sk);
    if(!list_empty(&ctsock->rbuf))
    {
        list_for_each_entry_safe(rbuf, temp, &ctsock->rbuf, node)
        {
            list_del(&rbuf->node);
						for(seq = 0; seq < rbuf->count; seq++)
						{
							if(NULL != rbuf->array[seq])
							{
								kfree_skb(rbuf->array[seq]);
							}
						}
            kfree(rbuf);
            printk("csock::release_sock_rbuf: close sock and release sock rbuf\n");
        }
    }
}



