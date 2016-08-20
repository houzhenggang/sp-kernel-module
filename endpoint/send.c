/*************************************************************************
	> File Name: send.h
	> Author: zzfan
	> Mail: zzfan@mail.ustc.edu.cn
	> Created Time: Wed 11 May 2016 05:35:21 AM PDT
 ************************************************************************/
#include "send.h"

int send_packet(struct sk_buff* skb)
{
    unsigned char nret = 1;
    //send the skb,每次发送增加引用计数
    //发完之后会kfree
    if(0 > dst_output(skb_clone(skb, GFP_ATOMIC)))
        goto out;
    nret = 0;
    //printk("Send::send_packet: send skb\n");

out:
    if(0 != nret && NULL !=skb)
    {
//        kfree_skb(skb);
    }
    return nret;
}


/*
 * 发送一个请求设置定时器:
 * 1.返回了ack,receive取消定时器
 * 2.超时重发
 */
int send_req(struct sock* sk, struct in6_addr saddr, struct in6_addr daddr, const char label[])
{
    int nret = 1;
    //创建req包
    struct sk_buff* skb = create_req(saddr, daddr, label);

    if(0 != send_packet(skb))
        goto out;
    nret = 0;

    //构造chunk,注意是目标地址
    req_send_add(skb, label, daddr);

out:
    if(0 != nret && NULL !=skb)
    {
        kfree_skb(skb);
        //printk("free send skb\n");
    }
    return nret;
}

/*
 * 分片发送,发送完了,发送hsyn
 */

int send_resp(struct sock* sk, struct in6_addr saddr, struct in6_addr daddr, const char label[], const char* buff, const unsigned int len)
{
    unsigned int offset = 0, bm_len, bits_left, i;
    uint16_t sequence = 0, frag_num = 0;
    unsigned int skb_data_len = 0; //如果buff小于PAYLAOD_SIZE
    int nret = 1;
    struct sk_buff* skb = NULL;

    //发送完数据包之后发送hsyn包
    struct sk_buff* hsyn_skb = NULL;

    //发送的时候这里构造chunk,不同于请求包
    struct content_chunk* chunk = NULL;

    //计算分片数
    while(offset < len){
        offset = offset + PAYLOAD_SIZE;
        ++frag_num;
    }

    //还有hsyn包也放在最后
    frag_num = frag_num + 1;
    offset = 0;

    //构造chunk
    chunk = kmalloc(sizeof(struct content_chunk) + frag_num*sizeof(struct sk_buff*), GFP_ATOMIC);
    chunk->sip = daddr;
    chunk->type = CHUNK_SEND;
    chunk->timer_count = 0;
    chunk->count = frag_num;
    memcpy(chunk->label, label, LABEL_SIZE);

		//计算bitmap数组长度, 以字节为单位
		bm_len = (chunk->count - 1 + 7)/8;
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

    //构造skb
    while(offset < len){
        nret = 1;

        //判断是否一个包装的下
        if(len - offset < PAYLOAD_SIZE)
            skb_data_len = len - offset;
        else
            skb_data_len = PAYLOAD_SIZE;
        skb = create_resp(saddr, daddr, label, sequence, frag_num-1, buff, offset, skb_data_len);

        //如果没有发送成功则跳出,并且销毁chunk和skb
        if(0 != send_packet(skb)){
            kfree_skb(skb);
            goto out;
        }
        nret = 0;

        //把skb放到chunk里面
        chunk->array[sequence] = skb;

        ++sequence;
        offset = offset + PAYLOAD_SIZE;
    }

    //发送hsyn包
    hsyn_skb = create_hsyn(saddr, daddr, label);
    if(0 != send_packet(hsyn_skb)){
        kfree_skb(hsyn_skb);
        goto out;
    }
    chunk->array[sequence] = hsyn_skb;

    //把chunk添加到表中,设置定时器
    resp_send_add(chunk);

out:
    if(0 != nret && NULL != chunk)
    {
        //free chunk and skb
        chunk_table_erase(&chunk_table, chunk);
        release_chunk(chunk);
    }
    return nret;
}

/*
 * 设置定时器
 * 0: 成功
 * -1: 没找到chunk
 *  1: 发送失败
 */
int send_ack(struct in6_addr saddr, struct in6_addr daddr, const char label[])
{
    int nret = 1;
    struct sk_buff* skb = create_ack(saddr, daddr, label);
    if(0 != send_packet(skb))
        goto out;
    nret = 0;

    //发送完ack后设置time_wait
    nret = ack_send_add(label, daddr);
    if(nret != 0){
        //ack for request
        //在表里面没有找到chunk
        return -1;
    }

out:
//    if(0 != nret && NULL !=skb)
    {
        kfree_skb(skb);
    }
    return nret;
}

/*
 * 发送hsyn包
 */

int send_hsyn(struct in6_addr saddr, struct in6_addr daddr, const char label[])
{
    int nret = 1;
    struct sk_buff* skb = create_hsyn(saddr, daddr, label);
    if(0 != send_packet(skb))
        goto out;
    nret = 0;

out:
    if(0 != nret && NULL !=skb)
    {
        kfree_skb(skb);
    }
    return nret;
}

/*
 * 发送needmore包
 * 0: success
 * -1: fail find chunk
 *  1: fail send packet
 */
int send_needmore(struct in6_addr saddr, struct in6_addr daddr, const char label[], unsigned char* bitmap, uint16_t len)
{
    int nret = 1;
    struct sk_buff* skb = create_needmore(saddr, daddr, label, bitmap, len);

    printk("Send::send_needmore: send needmore\n");
    if(0 != send_packet(skb))
        goto out;
    nret = 0;

    nret = needmore_send_add(label, daddr);
    if(nret < 0)
        return -1;

out:
    if(0 != nret && NULL !=skb)
    {
        kfree_skb(skb);
    }
    return nret;
}

/*
 * 构造response skb
 */
struct sk_buff* create_resp(struct in6_addr saddr, struct in6_addr daddr, const char label[], uint16_t sequence, uint16_t frag_num, const char* buff, const unsigned int offset, const unsigned int skb_data_len)
{
    struct sk_buff* skb = NULL;
    struct ethhdr* ethdr = NULL;
    struct ipv6hdr* iph = NULL;
    u_char* pexhdr = NULL;
    u_char* pdata = NULL;

    //分配skb空间给请求包
    skb = alloc_skb(sizeof(struct ipv6hdr) + LINK_LEN + sizeof(struct content_response) + skb_data_len, GFP_ATOMIC);

    //set skb
    skb_reserve(skb,LINK_LEN);
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(ETH_P_IPV6);
    skb->ip_summed = CHECKSUM_NONE;
    skb->priority = 0;

    //network hedaer = skb->data + 0
    skb_set_network_header(skb,0);
    //change tail and len pointer
    skb_put(skb,sizeof(struct ipv6hdr));

    //set ipv6 ex header poniter
    pexhdr = skb->data + sizeof(struct ipv6hdr);
    skb_put(skb, sizeof(struct content_response));

    //payload添加
    //printk("In create_resp:: payload size:%d\n", skb_data_len);
    pdata = pexhdr + sizeof(struct content_response);
    skb_put(skb, skb_data_len);
    {
        if(NULL != buff)
            copy_from_user(pdata, buff+offset, skb_data_len);
    }

    {
        //set ex ipv6 header content
        struct content_response resp;
        resp.head.nexthdr = 59;
        resp.head.length = 6;
        resp.head.version = 1;
        resp.head.FP = 1;
        resp.head.type = CONTENT_TYPE_RESP;
        resp.head.checksum = htons(0);
        memset(resp.head.pad, 0, 2);
        resp.sequence = htons(sequence);
        resp.frag_num = htons(frag_num);

        memcpy(resp.label, label, LABEL_SIZE);
        memset(resp.pad, 0, 4);
        //write into skb
        memcpy(pexhdr, &resp, sizeof(struct content_response));
    }

    {
        //set ipv6 header content
        iph = ipv6_hdr(skb);
        iph->version = 6;
        iph->priority = 0;
        memset(iph->flow_lbl,0,3);
        iph->payload_len = htons(sizeof(struct content_response) + skb_data_len);
        iph->nexthdr = IPPROTO_CONTENT;
        iph->hop_limit = 255;
        iph->saddr = saddr;
        iph->daddr = daddr;
    }

    {
        //set mac header content
        ethdr = (struct ethhdr*)(skb->data - 14);
        ethdr->h_proto = __constant_htons(ETH_P_IPV6);
    }

    if(skb_dst(skb) == NULL)
    {
        struct dst_entry* route = NULL;
        struct flowi6 fl6 =
        {
            .daddr = iph->daddr,
            .saddr = iph->saddr,
        };
        route = ip6_route_output(&init_net,NULL,&fl6);
        skb_dst_set(skb,route);
        skb->dev = route->dev;
        //printk("%s\n", skb->dev->name);
    }

    return skb;
}


struct sk_buff* create_hsyn(struct in6_addr saddr, struct in6_addr daddr, const char label[])
{
    struct sk_buff* skb = NULL;
    struct ethhdr* ethdr = NULL;
    struct ipv6hdr* iph = NULL;
    u_char* pexhdr = NULL;


    //分配skb空间给请求包
    skb = alloc_skb(sizeof(struct ipv6hdr) + LINK_LEN + sizeof(struct content_hsyn), GFP_ATOMIC);

    //set skb
    skb_reserve(skb,LINK_LEN);
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(ETH_P_IPV6);
    skb->ip_summed = CHECKSUM_NONE;
    skb->priority = 0;

    //network hedaer = skb->data + 0
    skb_set_network_header(skb,0);
    //change tail and len pointer
    skb_put(skb,sizeof(struct ipv6hdr));

    //set ipv6 ex header poniter
    pexhdr = skb->data + sizeof(struct ipv6hdr);
    skb_put(skb, sizeof(struct content_hsyn));

    {
        //set ex ipv6 header content
        struct content_hsyn hsyn;
        hsyn.head.nexthdr = 59;
        hsyn.head.length = 5;
        hsyn.head.version = 1;
        hsyn.head.FP = 1;
        hsyn.head.type = CONTENT_TYPE_HSYN;
        hsyn.head.checksum = htons(0);
        memset(hsyn.head.pad, 0, 2);

        memcpy(hsyn.label, label, LABEL_SIZE);
        //write into skb
        memcpy(pexhdr, &hsyn, sizeof(struct content_hsyn));
    }

    {
        //set ipv6 header content
        iph = ipv6_hdr(skb);
        iph->version = 6;
        iph->priority = 0;
        memset(iph->flow_lbl,0,3);
        iph->payload_len = htons(sizeof(struct content_hsyn));
        iph->nexthdr = IPPROTO_CONTENT;
        iph->hop_limit = 255;
        iph->saddr = saddr;
        iph->daddr = daddr;
    }

    {
        //set mac header content
        ethdr = (struct ethhdr*)(skb->data - 14);
        ethdr->h_proto = __constant_htons(ETH_P_IPV6);
    }

    if(skb_dst(skb) == NULL)
    {
        struct dst_entry* route = NULL;
        struct flowi6 fl6 =
        {
            .daddr = iph->daddr,
            .saddr = iph->saddr,
        };
        route = ip6_route_output(&init_net,NULL,&fl6);
        skb_dst_set(skb,route);
        skb->dev = route->dev;
        //printk("%s\n", skb->dev->name);
    }

    return skb;
}

struct sk_buff* create_req(struct in6_addr saddr, struct in6_addr daddr, const char label[])
{
    struct sk_buff* skb = NULL;
    struct ethhdr* ethdr = NULL;
    struct ipv6hdr* iph = NULL;
    u_char* pexhdr = NULL;


    //分配skb空间给请求包
    skb = alloc_skb(sizeof(struct ipv6hdr) + LINK_LEN + sizeof(struct content_request), GFP_ATOMIC);

    //set skb
    skb_reserve(skb,LINK_LEN);
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(ETH_P_IPV6);
    skb->ip_summed = CHECKSUM_NONE;
    skb->priority = 0;

    //network hedaer = skb->data + 0
    skb_set_network_header(skb,0);
    //change tail and len pointer
    skb_put(skb,sizeof(struct ipv6hdr));

    //set ipv6 ex header poniter
    pexhdr = skb->data + sizeof(struct ipv6hdr);
    skb_put(skb, sizeof(struct content_request));

    {
        //set ex ipv6 header content
        struct content_request req;
        req.head.nexthdr = 59;
        req.head.length = 5;
        req.head.version = 1;
        req.head.FP = 1;
        req.head.type = CONTENT_TYPE_REQ;
        req.head.checksum = htons(0);
        memset(req.head.pad, 0, 2);

        memcpy(req.label, label, LABEL_SIZE);
        //write into skb
        memcpy(pexhdr, &req, sizeof(struct content_request));
    }

    {
        //set ipv6 header content
        iph = ipv6_hdr(skb);
        iph->version = 6;
        iph->priority = 0;
        memset(iph->flow_lbl,0,3);
        iph->payload_len = htons(sizeof(struct content_request));
        iph->nexthdr = IPPROTO_CONTENT;
        iph->hop_limit = 255;
        iph->saddr = saddr;
        iph->daddr = daddr;
    }

    {
        //set mac header content
        ethdr = (struct ethhdr*)(skb->data - 14);
        ethdr->h_proto = __constant_htons(ETH_P_IPV6);
    }

    if(skb_dst(skb) == NULL)
    {
        struct dst_entry* route = NULL;
        struct flowi6 fl6 =
        {
            .daddr = iph->daddr,
            .saddr = iph->saddr,
        };
        route = ip6_route_output(&init_net,NULL,&fl6);
        skb_dst_set(skb,route);
        skb->dev = route->dev;
        //printk("%s\n", skb->dev->name);
    }

    return skb;
}

//创建ack
struct sk_buff* create_ack(struct in6_addr saddr, struct in6_addr daddr, const char label[])
{
    struct sk_buff* skb = NULL;
    struct ethhdr* ethdr = NULL;
    struct ipv6hdr* iph = NULL;
    u_char* pexhdr = NULL;


    //分配skb空间给请求包
    skb = alloc_skb(sizeof(struct ipv6hdr) + LINK_LEN + sizeof(struct content_ack), GFP_ATOMIC);

    //set skb
    skb_reserve(skb,LINK_LEN);
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(ETH_P_IPV6);
    skb->ip_summed = CHECKSUM_NONE;
    skb->priority = 0;

    //network hedaer = skb->data + 0
    skb_set_network_header(skb,0);
    //change tail and len pointer
    skb_put(skb,sizeof(struct ipv6hdr));

    //set ipv6 ex header poniter
    pexhdr = skb->data + sizeof(struct ipv6hdr);
    skb_put(skb, sizeof(struct content_ack));

    {
        //set ex ipv6 header content
        struct content_ack ack;
        ack.head.nexthdr = 59;
        ack.head.length = 5;
        ack.head.version = 1;
        ack.head.FP = 1;
        ack.head.type = CONTENT_TYPE_ACK;
        ack.head.checksum = htons(0);
        memset(ack.head.pad, 0, 2);

        memcpy(ack.label, label, LABEL_SIZE);
        //write into skb
        memcpy(pexhdr, &ack, sizeof(struct content_ack));
    }

    {
        //set ipv6 header content
        iph = ipv6_hdr(skb);
        iph->version = 6;
        iph->priority = 0;
        memset(iph->flow_lbl,0,3);
        iph->payload_len = htons(sizeof(struct content_ack));
        iph->nexthdr = IPPROTO_CONTENT;
        iph->hop_limit = 255;
        iph->saddr = saddr;
        iph->daddr = daddr;
    }

    {
        //set mac header content
        ethdr = (struct ethhdr*)(skb->data - 14);
        ethdr->h_proto = __constant_htons(ETH_P_IPV6);
    }

    if(skb_dst(skb) == NULL)
    {
        struct dst_entry* route = NULL;
        struct flowi6 fl6 =
        {
            .daddr = iph->daddr,
            .saddr = iph->saddr,
        };
        route = ip6_route_output(&init_net,NULL,&fl6);
        skb_dst_set(skb,route);
        skb->dev = route->dev;
        //printk("%s\n", skb->dev->name);
    }

    return skb;
}


struct sk_buff* create_needmore(struct in6_addr saddr, struct in6_addr daddr, const char label[], unsigned char* bitmap, uint16_t len)
{
    struct sk_buff* skb = NULL;
    struct ethhdr* ethdr = NULL;
    struct ipv6hdr* iph = NULL;
    u_char* pexhdr = NULL;
    uint8_t* pdata = NULL;
    uint16_t* pbitlen = NULL;


    //分配skb空间给请求包
    skb = alloc_skb(sizeof(struct ipv6hdr) + LINK_LEN + sizeof(struct content_needmore) + len + 2, GFP_ATOMIC);

    //set skb
    skb_reserve(skb,LINK_LEN);
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = __constant_htons(ETH_P_IPV6);
    skb->ip_summed = CHECKSUM_NONE;
    skb->priority = 0;

    //network hedaer = skb->data + 0
    skb_set_network_header(skb,0);
    //change tail and len pointer
    skb_put(skb,sizeof(struct ipv6hdr));

    //set ipv6 ex header poniter
    pexhdr = skb->data + sizeof(struct ipv6hdr);
    skb_put(skb, sizeof(struct content_needmore));

    //set bitmap_len and bitmap在payload
    pbitlen = (uint16_t*)(pexhdr + sizeof(struct content_needmore));
    pdata = (uint8_t*)(pexhdr + sizeof(struct content_needmore) + 2);
    skb_put(skb, len+2);

    {
        *pbitlen = htons(len);
        if(NULL != bitmap)
            memcpy(pdata, bitmap, len);
    }

    {
        //set ex ipv6 header content
        struct content_needmore needmore;
        needmore.head.nexthdr = 59;
        needmore.head.length = 5;
        needmore.head.version = 1;
        needmore.head.FP = 1;
        needmore.head.type = CONTENT_TYPE_NEEDMORE;
        needmore.head.checksum = htons(0);
        memset(needmore.head.pad, 0, 2);

        memcpy(needmore.label, label, LABEL_SIZE);
        //write into skb
        memcpy(pexhdr, &needmore, sizeof(struct content_needmore));
    }

    {
        //set ipv6 header content
        iph = ipv6_hdr(skb);
        iph->version = 6;
        iph->priority = 0;
        memset(iph->flow_lbl,0,3);
        iph->payload_len = htons(sizeof(struct content_needmore) + len + 2);
        iph->nexthdr = IPPROTO_CONTENT;
        iph->hop_limit = 255;
        iph->saddr = saddr;
        iph->daddr = daddr;
    }

    {
        //set mac header content
        ethdr = (struct ethhdr*)(skb->data - 14);
        ethdr->h_proto = __constant_htons(ETH_P_IPV6);
    }

    if(skb_dst(skb) == NULL)
    {
        struct dst_entry* route = NULL;
        struct flowi6 fl6 =
        {
            .daddr = iph->daddr,
            .saddr = iph->saddr,
        };
        route = ip6_route_output(&init_net,NULL,&fl6);
        skb_dst_set(skb,route);
        skb->dev = route->dev;
        //printk("%s\n", skb->dev->name);
    }

    return skb;
}


struct in6_addr get_saddr(struct sock* sk, struct in6_addr daddr)
{
    struct in6_addr saddr;
    struct flowi6 fl6;
    struct dst_entry* dst;
    int addr_type;
    saddr = inet6_sk(sk)->saddr;

    addr_type = ipv6_addr_type(&daddr);

    memset(&fl6, 0, sizeof(fl6));
    fl6.saddr = saddr;
    fl6.daddr = daddr;

    dst = ip6_route_output(sock_net(sk), sk, &fl6);

    if (dst) {
        struct rt6_info* rt;
        struct inet6_dev* idev;
        int err = 0;
        //printk(KERN_DEBUG "route success\n");
        rt = dst->error ? NULL : (struct rt6_info*)dst;

        idev = ip6_dst_idev(dst);
        err = ipv6_dev_get_saddr(sock_net(sk), idev ? idev->dev : NULL, &daddr,
                                 inet6_sk(sk)->srcprefs, &saddr);
        if (err) {
            printk(KERN_ERR "ip6_route_get_saddr fail\n");
        }
    }

    /*printk(KERN_DEBUG "saddr: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
                   htons(saddr.in6_u.u6_addr16[0]), htons(saddr.in6_u.u6_addr16[1]),
                   htons(saddr.in6_u.u6_addr16[2]), htons(saddr.in6_u.u6_addr16[3]),
                   htons(saddr.in6_u.u6_addr16[4]), htons(saddr.in6_u.u6_addr16[5]),
                   htons(saddr.in6_u.u6_addr16[6]), htons(saddr.in6_u.u6_addr16[7]));

            printk(KERN_DEBUG "daddr: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
                   htons(daddr.in6_u.u6_addr16[0]), htons(daddr.in6_u.u6_addr16[1]),
                   htons(daddr.in6_u.u6_addr16[2]), htons(daddr.in6_u.u6_addr16[3]),
                   htons(daddr.in6_u.u6_addr16[4]), htons(daddr.in6_u.u6_addr16[5]),
                   htons(daddr.in6_u.u6_addr16[6]), htons(daddr.in6_u.u6_addr16[7]));
*/
    return saddr;
}
