/*
 *写一个TCP回射程序，程序的功能是：客户端向服务器发送信息，服务器接收并原样发送给客户端，客户端显示出接收到的信息。
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

#define IPADDR	"192.168.1.42"
#define PORT	8787
#define MAXLINE	1024
#define LISTENQ	5	//未完成队列的大小
#define SIZE	10

typedef struct server_context_st
{
	int cli_cnt;		//客户端个数
	int clifds[SIZE];	//客户端个数
	fd_set allfds;		//句柄集合
	int maxfd;			//句柄最大值
}server_context_st;

static server_context_st *s_srv_ctx = NULL;


static int create_server_proc(const char *ip,int port)
{
	//创建一个监听套接字
	int fd;
	struct sockaddr_in servaddr;
	fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd == -1){
		fprintf(stderr,"create socket fail,erron:%d,reason:%s\n",errno,strerror(errno));
		return -1;
	}
	//一个端口释放后会等待两分钟之后擦能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用
	int reuse = 1;
	if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) == -1){
		return -1;
	}
	//清零初始化
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET,ip,&servaddr.sin_addr);
	servaddr.sin_port = htons(port);
	
	if(bind(fd,(struct sockaddr*)&servaddr,sizeof(servaddr)) == -1){
		perror("bind error: ");
		return -1;
	}
	listen(fd,LISTENQ);

	return fd;
}

static int accept_client_proc(int srvfd)
{
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	cliaddrlen = sizeof(cliaddr);
	int clifd = -1;

	printf("accept client proc is called.\n");

ACCEPT:
	//等待连接
	clifd = accept(srvfd,(struct sockaddr*)&cliaddr,&cliaddrlen);

	if(clifd == -1){
		if(errno == EINTR){
			goto ACCEPT;
		}else{
			fprintf(stderr,"accept fail,error:%s\n",strerror(errno));
			return -1;
		}
	}
	fprintf(stdout,"accept a new client: %s:%d\n",inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);

	//将新的连接描述符添加到数组中
	int i = 0;
	for(i = 0;i < SIZE;i++){
		if(s_srv_ctx->clifds[i] < 0){
			s_srv_ctx->clifds[i] = clifd;
			s_srv_ctx->cli_cnt++;
			break;
		}
	}
	if(i == SIZE){
		fprintf(stderr,"too mang clients.\n");
		return -1;
	}
}

static int handle_client_msg(int fd,char *buf)
{
	assert(buf);
	printf("recv buf is:%s\n",buf);
	write(fd,buf,strlen(buf)+1);
	return 0;
}

static void recv_client_msg(fd_set *readfds)
{
	int i = 0,n = 0;
	int clifd;
	char buf[MAXLINE] = {0};
	for(i = 0;i < s_srv_ctx->cli_cnt;i++){
		clifd = s_srv_ctx->clifds[i];
		if(clifd < 0){
			continue;
		}
		/*判断客户端套接字是否有数据*/
		if(FD_ISSET(clifd,readfds)){
			//接收客户端发送的消息
			n = read(clifd,buf,MAXLINE);
			if(n <= 0){
				//n == 0 表示读取完成，客户都关闭套接字
				FD_CLR(clifd,&s_srv_ctx->allfds);	//将一个给定的文件描述符从集合中删除
				close(clifd);
				s_srv_ctx->clifds[i] == -1;
				continue;
			}
			handle_client_msg(clifd,buf);
		}
	}
}

static void handle_client_proc(int srvfd)
{
	int clifd = -1;
	int retval = 0;
	fd_set *readfds = &s_srv_ctx->allfds;
	struct timeval tv;
	int i = 0;

	while(1){
		//每次调用select前都要重新设置文件描述符和时间，因为时间事件发生后，文件描述符和时间都被内核修改了
		FD_ZERO(readfds);	//清空集合
		//添加监听套接字
		FD_SET(srvfd,readfds);//将一个给定的文件描述法加入集合
		s_srv_ctx->maxfd = srvfd;//文件描述符是递增增长的,所以把当前接收到的客户端文件描述符作为最大文件描述符

		tv.tv_sec = 30;//30s
		tv.tv_usec = 0;
		//添加客户端套接字到readfds集合中
		for(i = 0;i < s_srv_ctx->cli_cnt;i++){//遍历客户端 cli_cnt--客户端个数
			clifd = s_srv_ctx->clifds[i];
			//去除无效的客户端句柄
			if(clifd != -1){
				FD_SET(clifd,readfds);
			}
			s_srv_ctx->maxfd = (clifd > s_srv_ctx->maxfd ? clifd : s_srv_ctx->maxfd);
		}
		//开始轮训接收处理服务端和客户端套接字
		//会遍历传递进来的所有fd,默认最大为1024
		retval = select(s_srv_ctx->maxfd + 1,readfds,NULL,NULL,&tv);
		if(retval == -1){//出错
			fprintf(stderr, "select error:%s\n",strerror(errno));
			return;
		}
		if(retval == 0){//超时
			fprintf(stdout,"select is timeout\n");
			continue;
		}
		//检查集合中指定的文件描述符是否可以读写
		if(FD_ISSET(srvfd,readfds)){//检查srvfd(服务端)文件描述符是否可以读写，证明有客户端连接请求过来，//将新的连接描述符添加到数组中
			//监听客户端请求
			accept_client_proc(srvfd);
		}else{
			//接收处理客户端消息
			recv_client_msg(readfds);
		}
	}
}

static void server_uninit()
{
	if(s_srv_ctx){
		free(s_srv_ctx);
		s_srv_ctx = NULL;
	}
}

static int server_init()
{
	//动态分配空间
	s_srv_ctx = (server_context_st *)malloc(sizeof(server_context_st));
	if(s_srv_ctx == NULL){
		return -1;
	}
	//对分配的空间进行清零初始化
	memset(s_srv_ctx,0,sizeof(server_context_st));
	//对保存客户端描述符的数组初始化为-1
	int i = 0;
	for(i = 0;i < SIZE; i++){
		s_srv_ctx->clifds[i] = -1;
	}

	return 0;
}

int main(int argc,char *argv[])
{
	int srvfd;
	//初始化服务器端context
	if(server_init() < 0){
		return -1;
	}
	//创建服务，开始监听客户端请求
	srvfd = create_server_proc(IPADDR,PORT);
	if(srvfd < 0){
		fprintf(stderr, "socket create or bind fail.\n");
		goto err;
	}
	//开始接收并处理客户端请求
	handle_client_proc(srvfd);
	server_uninit();
	return 0;
err:
	server_uninit();
	return -1;
}


