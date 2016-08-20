/*******************************************************************************

 File Name: kernel.c
 Author: Shaopeng Li
 mail: lsp001@mail.ustc.edu.cn
 Created Time: 2016年04月20日 星期三 14时01分32秒

*******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ipv6.h>

#include <net/inet_common.h>
#include <net/ipv6.h>
#include <net/sock.h>
#include <net/protocol.h>

#include "ctable.h"
#include "csock.h"
#include "trans.h"
#include "proto.h"
#include "recv.h"
#include "send.h"

MODULE_AUTHOR("lsp");
MODULE_LICENSE("GPL");

static int content_rcv(struct sk_buff* skb)
{
	int ret = 1;
	struct content_hdr* content_hdr = (struct content_hdr*)(skb->data);

	switch(content_hdr->type)
	{
		case CONTENT_TYPE_REQ:
			ret = recv_req(skb);
            printk("PACKET TYPE: REQ!\n");
			break;
		case CONTENT_TYPE_RESP:
            //printk("PACKET TYPE: RESP!\n");
			ret = recv_resp(skb);
			break;
		case CONTENT_TYPE_HSYN:
            printk("PACKET TYPE: HSYN!\n");
			ret = recv_hsyn(skb);
			 break;
		case CONTENT_TYPE_ACK:
            printk("PACKET TYPE: ACK!\n");
			ret = recv_ack(skb);
			break;
		case CONTENT_TYPE_REJECT:
			//recv_reject(skb);
			break;
		case CONTENT_TYPE_NEEDMORE:
            printk("PACKET TYPE: NEEDMORE!\n");
			ret = recv_needmore(skb);
			break;
		default:
			//kfree_skb(skb);
			printk(KERN_DEBUG "content packets do not have this type!\n");
			return -1;
	}
	if(ret == 0) return -1;
	return 0;
}

static struct inet6_protocol content_protocol = {
	.handler = content_rcv,
};

static void content_hash(struct sock* sk)
{
	BUG();
}

static void content_unhash(struct sock* sk)
{
	printk(KERN_DEBUG "content_unhash\n");
	//套接字销毁时,从content_table里删除对应sock
}

static int content_get_port(struct sock* sk, unsigned short num)
{
	printk(KERN_DEBUG "content shouldn't bind port %d\n", num);
	return 1; //调用bind port时,端口始终"被占用"
}

static struct proto content_proto = {
	.name = "CONTENT",
	.owner = THIS_MODULE,
	.init = csock_init,
	.close = csock_fini,
	.connect = NULL,
	.disconnect = NULL,
	.ioctl = NULL,
	.destroy = NULL,
	.setsockopt = csock_setsockopt,
	.getsockopt = csock_getsockopt,
	.sendmsg = csock_sendmsg,
	.recvmsg = csock_recvmsg,
	.backlog_rcv = NULL,
	.no_autobind = 1,
	.hash = content_hash,
	.unhash = content_unhash,
	.rehash = NULL,
	.get_port = content_get_port,
	.memory_allocated = NULL,
	.sysctl_mem = NULL,
	.sysctl_rmem = NULL,
	.sysctl_wmem = NULL,
	.obj_size = sizeof(struct content_sock),
	.slab_flags = SLAB_DESTROY_BY_RCU,
	.h.hashinfo = (struct inet_hashinfo*)&content_table,
};


static struct proto_ops content_dgram_ops = {
	.family = PF_INET6,
	.owner = THIS_MODULE,
	.release	   = inet6_release,   //--> proto.close
	 //bind_address_no_port
	.bind		   = inet6_bind,
	.connect	   = sock_no_connect,	/* ok		*/
	.socketpair	   = sock_no_socketpair,	/* a do nothing	*/
	.accept		   = sock_no_accept,		/* a do nothing	*/
	.getname	   = inet6_getname,
	.poll		   = sock_no_poll,			/* ok		*/
	.ioctl		   = inet6_ioctl,		/* must change  */
	.listen		   = sock_no_listen,		/* ok		*/
	.shutdown	   = inet_shutdown,		/* ok		*/
	.setsockopt	   = sock_common_setsockopt,	/* ok		*/
	.getsockopt	   = sock_common_getsockopt,	/* ok		*/
	.sendmsg	   = inet_sendmsg,		/* ok		*/
	.recvmsg	   = inet_recvmsg,		/* ok		*/
	.mmap		   = sock_no_mmap,
	.sendpage	   = sock_no_sendpage,
};

static struct inet_protosw content_protosw = {
	.type = SOCK_DGRAM,
	.protocol = IPPROTO_CONTENT,
	.prot = &content_proto,
	.ops = &content_dgram_ops,
};


static int __init content_sock_init(void)
{
	int ret;
	ret = proto_register(&content_proto, 1);
	if (ret) {
		return ret;
	}
	ret = inet6_add_protocol(&content_protocol, IPPROTO_CONTENT);
	if (ret) {
		goto unregister_proto;
	}
	ret = inet6_register_protosw(&content_protosw);
	if (ret) {
		goto del_protocol;
	}
	content_table_init(&content_table, "CONTENT");
	chunk_table_init(&chunk_table, "CHUNK");
	return ret;

 del_protocol:
	inet6_del_protocol(&content_protocol, IPPROTO_CONTENT);
 unregister_proto:
	proto_unregister(&content_proto);
	return ret;
}

static void __exit content_sock_exit(void)
{
	proto_unregister(&content_proto);
	inet6_unregister_protosw(&content_protosw);
	inet6_del_protocol(&content_protocol, IPPROTO_CONTENT);
	content_table_fini(&content_table);
	chunk_table_fini(&chunk_table);
}

module_init(content_sock_init);
module_exit(content_sock_exit);
