/*
 *
 *编写一个echo server程序，功能是客户端向服务器发送信息，服务器接收输出并原样发送回给客户端，客户端接收到输出到终端。
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define IPADRESS	"192.168.1.42"
#define PORT		8787
#define	MAXLINE		1024
#define	LISTENQ		5
#define OPEN_MAX	1000
#define INFTIM		-1

/*************函数声明******************/
//创建套接字并进行绑定
static int socket_bind(const char *ip,int port);
//IO多路复用POLL
static void do_poll(int listenfd);
//处理多个连接
static void handle_connection(struct pollfd *confds,int num);

int main(int argc,char *argv[])
{
	int listenfd,connfd,sockfd;
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	listenfd = socket_bind(IPADRESS,PORT);
	listen(listenfd,LISTENQ);
	printf("服务器开始.\n");
	do_poll(listenfd);
	return 0;
}

static int socket_bind(const char *ip,int port)
{
	int listenfd;
	struct sockaddr_in servaddr;
	listenfd = socket(AF_INET,SOCK_STREAM,0);
	if(listenfd == -1){
		perror("socket error: ");
		exit(1);
	}

	/*一个端口释放后会等待两分钟之后才能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用。*/
	int reuse = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        return -1;
    }


	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	if(bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) == -1){
		perror("bind error: ");
		exit(1);
	}
	return listenfd;
}
static void do_poll(int listenfd)
{
	int connfd,sockfd;
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	struct pollfd clientfds[OPEN_MAX];
	int maxi;
	int i;
	int nready;
	//添加监听描述符
	clientfds[0].fd = listenfd;
	clientfds[0].events = POLLIN;	//添加读事件
	//初始化客户连接描述符
	for(i = 1;i < OPEN_MAX;i++){
		clientfds[i].fd = -1;
	}
	maxi = 0;
	//循环处理
	for(;;)
	{
		//获取可用描述符个数
		//poll设置time为-1，表示无限超时，阻塞的。还可以指定0，调用后立刻返回，非阻塞，还可以设置指定的超时时间，例如10s。
		nready = poll(clientfds,maxi+1,INFTIM);
		if(nready == -1){
			perror("poll error: ");
			exit(1);
		}
		//测试监听描述符是否准备好
		if(clientfds[0].revents & POLLIN){
			cliaddrlen = sizeof(cliaddr);
			//接受新的连接
			if((connfd = accept(listenfd,(struct sockaddr*)&cliaddr,&cliaddrlen)) == -1){
				if(errno == EINTR)
					continue;
				else{
					perror("accept error:");
					exit(1);
				}
			}
			fprintf(stdout,"accept a new client: %s:%d\n",inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);
			//将新的连接描述符添加到数组
			for(i = 1;i < OPEN_MAX;i++){
				if(clientfds[i].fd < 0){
					clientfds[i].fd = connfd;
					break;
				}
			}
			if(i == OPEN_MAX){
				fprintf(stderr,"too many clients.\n");
				exit(1);
			}
			//将新的描述符 添加到读描述符集合中
			clientfds[i].events = POLLIN;
			//记录客户连接套接字的个数
			maxi = (i > maxi ? i : maxi);
			if(--nready <= 0){//当nready=1的时候，就表示select的返回值中只有监听套接字，不需要运行后面的for循环了。
				continue;
			}
		}
		//处理客户连接
		handle_connection(clientfds,maxi);
	}
}

static void handle_connection(struct pollfd *connfds,int num)
{
	int i,n;
	char buf[MAXLINE];
	memset(buf,0,MAXLINE);
	for(i = 1;i <= num;i++){
		if(connfds[i].fd < 0)
			continue;
		//测试客户描述符是否准备好
		if(connfds[i].revents & POLLIN){
			//接收客户端发送的信息
			n = read(connfds[i].fd,buf,MAXLINE);
			if(n == 0){
				close(connfds[i].fd);
				connfds[i].fd = -1;
				continue;
			}
			write(STDOUT_FILENO,buf,n);
			//向客户端发送buf
			write(connfds[i].fd,buf,n);
		}
	}
}



