/*******************************************************************************

 File Name: cli.c
 Author: Shaopeng Li
 mail: lsp001@mail.ustc.edu.cn
 Created Time: 2016年04月26日 星期二 10时02分24秒

*******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../proto.h"
char res[1024*1024*8];

int main()
{
    char getlabel[LABEL_SIZE];
    char labelset[5] = "abcd";
	int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_CONTENT);
	if (sock == -1) {
		perror("socket");
		return 1;
	}
    setsockopt(sock, 0, 0, labelset, 5);

	struct in6_addr myaddr = {{{0xfe, 0xc0, 0, 0xa,
								0,0,0,0,
								0,0,0,0,
								0, 0, 0, 3}}};

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
	struct content_label req_ctl = {{MSGCTL_SIZE, IPPROTO_CONTENT, CONTENT_REQ},
									{'a', 'b', 'c', 'd'}};

	bzero(&msg, sizeof(msg));

	msg.msg_name = &myaddr;
	msg.msg_namelen = sizeof(myaddr);
	msg.msg_control = &req_ctl;
	msg.msg_controllen = MSGCTL_SIZE;
	if (sendmsg(sock, &msg, 0) == -1) {
		perror("sendmsg");
	}

	bzero(&msg, sizeof(msg));
	struct in6_addr clientaddr;
	struct content_label resp_ctl = {{MSGCTL_SIZE, IPPROTO_CONTENT, 0}};
	iov.iov_base = res;
	iov.iov_len = sizeof(res);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = &clientaddr;
	msg.msg_namelen = sizeof(clientaddr);
	msg.msg_control = &resp_ctl;
	msg.msg_controllen = MSGCTL_SIZE;
	
	if (recvmsg(sock, &msg, 0) == -1) {
		perror("recvmsg");
	}
    memcpy(getlabel, LABEL(msg.msg_control), LABEL_SIZE);
    printf("show label: %s\n", ((struct content_label*)(msg.msg_control))->label);
    //printf("show data: %c\n", res[0]);
    //printf("msg_iovlen: %d\n", (int)msg.msg_iov->iov_len);
    printf("msg_iovbase: %.1050s\n", (char*)msg.msg_iov->iov_base);
    fflush(stdout);

	close(sock);
	return 0;
}
