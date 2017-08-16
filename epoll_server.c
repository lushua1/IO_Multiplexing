/*
*
*　编写一个服务器回射程序echo
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>

#define IPADDRESS	"127.0.0.1"
#define PORT		8787
#define MAXSIZE		1024
#define LISTENQ		5
#define FDSIZE		1000
#define EPOLLEVENTS	100

//函数声明
//创建套接字并进行绑定
static int socket_bind(const char *ip,int port);
//IO复用epoll
static void do_epoll(int listenfd);
//事件处理函数
static void handle_events(int epollfd,struct epoll_event *events,int num,int listenfd,char *buf);
//处理接收到的连接
static void handle_accept(int epollfd,int listenfd);
//读处理
static void do_read(int epollfd,int fd,char *buf);
//写处理
static void do_write(int epollfd,int fd,char *buf);
//添加事件
static void add_event(int epollfd,int fd,int state);
//修改事件
static void modify_event(int epollfd,int fd,int state);
//删除事件
static void delete_event(int epollfd,int fd,int state);

int main(int argc,char *argv[])
{
	int listenfd;
	listenfd = socket_bind(IPADDRESS,PORT);
	listen(listenfd,LISTENQ);
	printf("服务器开启.\n");
	do_epoll(listenfd);
	return 0;
}

static int socket_bind(const char *ip,int port)
{
	int listenfd;
	struct sockaddr_in servaddr;
	listenfd = socket(AF_INET,SOCK_STREAM,0);
	if(listenfd == -1)
	{
		perror("socket error:");
		exit(1);
	}
	//一个端口释放后会等待两分钟之后才能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用
	int reuse = 1;
	if(setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) == -1){
		return -1;
	}

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET,ip,&servaddr.sin_addr);
	if(bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1)
	{
		perror("bind error:");
		exit(1);
	}
	return listenfd;
}

static void do_epoll(int listenfd)
{
	int epollfd;
	struct epoll_event events[EPOLLEVENTS];
	int ret;
	char buf[MAXSIZE];
	memset(buf,0,MAXSIZE);
	//创建一个描述符
	epollfd = epoll_create(FDSIZE);
	//添加监听描述符事件
	add_event(epollfd,listenfd,EPOLLIN);
	for(;;)
	{
		//获取已经准备好的描述符事件
		ret = epoll_wait(epollfd,events,EPOLLEVENTS,-1);
		handle_events(epollfd,events,ret,listenfd,buf);
	}
	close(epollfd);
}

static void handle_events(int epollfd,struct epoll_event *events,int num,int listenfd,char *buf)
{
	int i;
	int fd;
	//进行选好遍历
	for(i = 0;i < num;i++)
	{
		fd = events[i].data.fd;
		//根据描述符的类型和时间类型进行处理
		if((fd == listenfd) && (events[i].events & EPOLLIN))//服务器监听事件
			handle_accept(epollfd,listenfd);
		else if(events[i].events & EPOLLIN)
			do_read(epollfd,fd,buf);
		else if(events[i].events & EPOLLOUT)
			do_write(epollfd,fd,buf);
	}
}
static void handle_accept(int epollfd,int listenfd)
{
	int clifd;
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);
	clifd = accept(listenfd,(struct sockaddr*)&cliaddr,&cliaddrlen);
	if(clifd == -1)
		perror("accept error: ");
	else
	{
		printf("accept a new client:%s:%d\n",inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);
		//添加一个客户描述符和事件
		add_event(epollfd,clifd,EPOLLIN);
	}
}

static void do_read(int epollfd,int fd,char *buf)
{
	int nread;
	nread = read(fd,buf,MAXSIZE);
	if(nread == -1)//读取出错
	{
		perror("read error:");
		close(fd);
		delete_event(epollfd,fd,EPOLLIN);
	}
	else if(nread == 0)	//超时
	{
		fprintf(stderr,"client close.\n");
		close(fd);
		delete_event(epollfd,fd,EPOLLIN);
	}
	else
	{
		printf("read message is : %s\n",buf);
		//修改描述符对应的事件，由读改为写
		modify_event(epollfd,fd,EPOLLOUT);
	}
}

static void do_write(int epollfd,int fd,char *buf)
{
	int nwrite;
	nwrite = write(fd,buf,strlen(buf));
	if(nwrite == -1)
	{
		perror("write error:");
		close(fd);
		delete_event(epollfd,fd,EPOLLOUT);
	}
	else
		modify_event(epollfd,fd,EPOLLIN);
	memset(buf,0,MAXSIZE);
}

static void add_event(int epollfd,int fd,int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);//EPOLL_CTL_ADD：注册新的fd到epfd中
}

static void delete_event(int epollfd,int fd,int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);//EPOLL_CTL_DEL：从epfd中删除一个fd
}

static void modify_event(int epollfd,int fd,int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&ev);//EPOLL_CTL_MOD：修改已经注册的fd的监听事件
}
