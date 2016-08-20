/*******************************************************************************

 File Name: srv.c
 Author: Shaopeng Li
 mail: lsp001@mail.ustc.edu.cn
 Created Time: 2016年04月26日 星期二 10时02分29秒

*******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../proto.h"
#define MAX (4*1024*1024)
char data[MAX];

int main()
{
    char getlabel[LABEL_SIZE];
    char labelset[5] = "abcd";

    FILE *fp = fopen("data", "r");
    fread(data, 1, MAX, fp);
    fclose(fp);
    //memset(data, 0xff, MAX);
    printf("just for test: %c\n", data[MAX-1]);

	int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_CONTENT);
	if (sock == -1) {
		perror("socket");
		return 1;
	}
    setsockopt(sock, 0, 0, labelset, 4);

    /*
    struct in6_addr myaddr = {{{0xfe, 0xc0, 0, 0xb,
                           0,0,0,0,
                           0,0,0,0,
                           0, 0, 0, 3}}};
                           */

    struct sockaddr_in6 saddr;
    bzero(&saddr, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_addr = in6addr_any;
    saddr.sin6_port = htons(0);

    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
        perror("bind");
    }

	struct iovec iov;
    struct msghdr msg;
    struct in6_addr clientaddr;
	
	bzero(&msg, sizeof(msg));
	struct content_label req_ctl = {{MSGCTL_SIZE, IPPROTO_CONTENT, 0}};
	msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = &clientaddr;
    msg.msg_namelen = sizeof(clientaddr);
	msg.msg_control = &req_ctl;
	msg.msg_controllen = MSGCTL_SIZE;
	
//    sleep(1);
while(1){
	if (recvmsg(sock, &msg, 0) == -1) {
		perror("recvmsg");
	}
	printf("myaddr: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
                   (htons((*((struct in6_addr*)(msg.msg_name))).s6_addr16[0])), htons(((*((struct in6_addr*)(msg.msg_name))).s6_addr16[1])),
                   (htons((*((struct in6_addr*)(msg.msg_name))).s6_addr16[2])), htons(((*((struct in6_addr*)(msg.msg_name))).s6_addr16[3])),
                   (htons((*((struct in6_addr*)(msg.msg_name))).s6_addr16[4])), htons(((*((struct in6_addr*)(msg.msg_name))).s6_addr16[5])),
                   (htons((*((struct in6_addr*)(msg.msg_name))).s6_addr16[6])), htons(((*((struct in6_addr*)(msg.msg_name))).s6_addr16[7])));
    memcpy(getlabel, LABEL(msg.msg_control), LABEL_SIZE);

	//if (req_ctl.hdr.cmsg_type == CONTENT_REQ && memcmp(getlabel, "abcd", 4) == 0){
	    bzero(&msg, sizeof(msg));
	    struct content_label resp_ctl = {{MSGCTL_SIZE, IPPROTO_CONTENT, CONTENT_RESP},
	    								 {'a', 'b', 'c', 'd'}};
	   //char res[] = {'r', 'e', 's', 'u', 'l', 't', '\0'};
       //iov.iov_base = res;
       //iov.iov_len = 7;
	   iov.iov_base = data;
	   iov.iov_len = MAX;
	   msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_name = &clientaddr;
        msg.msg_namelen = sizeof(struct in6_addr);
	    msg.msg_control = &resp_ctl;
	    msg.msg_controllen = MSGCTL_SIZE;
	    
	    if (sendmsg(sock, &msg, 0) == -1) {
	    	perror("sendmsg");
	    }
}

	close(sock);
	return 0;
}
