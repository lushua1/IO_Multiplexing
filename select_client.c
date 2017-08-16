#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#define MAXLINE	1024
#define IPADDRESS "192.168.1.42"
#define SERV_PORT 8787

#define max(a,b) (a > b) ? a : b

static void handle_recv_msg(int sockfd,char *buf)
{
	printf("client recv msg is: %s\n",buf);
	sleep(5);
	write(sockfd,buf,strlen(buf)+1);
}

static void handle_connection(int sockfd)
{
	char sendline[MAXLINE],recvline[MAXLINE];
	int maxfdp,stdineof;
	fd_set readfds;
	int n;
	struct timeval tv;
	int retval = 0;

	while(1){
		FD_ZERO(&readfds);		//清空集合
		FD_SET(sockfd,&readfds);	//将一个给定的文件描述符加入集合中
		maxfdp = sockfd;

		tv.tv_sec = 5;			//定时5s
		tv.tv_usec = 0;

		retval = select(maxfdp+1,&readfds,NULL,NULL,&tv);	

		if(retval == -1){
			return;
		}
		if(retval == 0){
			printf("client timeout.\n");
			continue;
		}
		if(FD_ISSET(sockfd,&readfds)){	//检查集合中指定的文件描述符是否可以读写
			n = read(sockfd,recvline,MAXLINE);
			if(n <= 0){
				fprintf(stderr,"client:server is closed.\n");
				close(sockfd);
				FD_CLR(sockfd,&readfds);	//将一个给定的文件描述符从集合中删除
				return;
			}
			handle_recv_msg(sockfd,recvline);
		}
	}
}


int main(int argc,char *argv[])
{
	int sockfd;
	struct sockaddr_in servaddr;
	sockfd = socket(AF_INET,SOCK_STREAM,0);

	bzero(&servaddr,sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);
	inet_pton(AF_INET,IPADDRESS,&servaddr.sin_addr);

	int retval = 0;
	retval = connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
	if(retval < 0){
		fprintf(stderr,"connect fail,error:%s\n",strerror(errno));
		return -1;
	}
	printf("client  send to server.\n");
	write(sockfd,"hello server",32);

	handle_connection(sockfd);
	return 0;
}
