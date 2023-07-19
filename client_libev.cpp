#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ev.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stddef.h>

#include "ikcp.h"

#ifndef container_of
#define container_of(ptr, type, member)					\
	({								\
		const __typeof__(((type *) NULL)->member) *__mptr = (ptr);	\
		(type *) ((char *) __mptr - offsetof(type, member));	\
	})
#endif

typedef struct KCPOBJ {
	unsigned char *ipstr;
	int port;
	ikcpcb *pkcp;
	int sockfd;
	struct sockaddr_in addr; //存放服务器的结构体
	char buff[488]; //存放收发的消息
    struct ev_io ior;
	ev_timer timeout_watcher;
} kcpObj;


/* get system time */
void itimeofday(long *sec, long *usec)
{
	#if defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(__APPLE__)
        struct timeval time;
        gettimeofday(&time, NULL);
        if (sec) *sec = time.tv_sec;
        if (usec) *usec = time.tv_usec;
	#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
        static long mode = 0, addsec = 0;
        int retval;
        static IINT64 freq = 1;
        IINT64 qpc;
        if (mode == 0) {
            retval = QueryPerformanceFrequency((int64_t*)&freq);
            freq = (freq == 0)? 1 : freq;
            retval = QueryPerformanceCounter((int64_t*)&qpc);
            addsec = (long)time(NULL);
            addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
            mode = 1;
        }
        retval = QueryPerformanceCounter((int64_t*)&qpc);
        retval = retval * 2;
        if (sec) *sec = (long)(qpc / freq) + addsec;
        if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

/* sleep in millisecond */
void isleep(unsigned long millisecond)
{
	#if defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(__APPLE__)
        struct timespec ts;
        ts.tv_sec = (time_t)(millisecond / 1000);
        ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
        /*nanosleep(&ts, NULL);*/
        usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
	    Sleep(millisecond);
	#endif
}

void print_hex(const char* title, const char *buf, int len){
    printf("==> %s [%d] <==\n", title, len);

    for (int i = 0; i < len; i++)
    {
        printf("%02X ", (unsigned char) buf[i]);
    }
    printf("\n");
}

void print_str(const char* title, const char *buf, size_t len){
    printf("==> %s [%zu] <==\n", title, len);

    for (int i = 0; i < len; i++)
    {
        printf("%c", (unsigned char) buf[i]);
    }
    printf("\n");
}


int udpOutPut(const char *buf, int len, ikcpcb *kcp, void *user){
    kcpObj *send = (kcpObj *)user;
	//发送信息
	// print_hex("udpOutPut", buf, len);
    int n = sendto(send->sockfd, buf, len, 0,(struct sockaddr *) &send->addr,sizeof(struct sockaddr_in));//【】
    if (n >= 0) 
	{
        return n;
    } 
	else 
	{
        printf("udpOutPut: %d bytes send, error\n", n);
        return -1;
    }
}


int init(kcpObj *send)
{	
	send->sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	
	if(send->sockfd < 0)
	{
		perror("socket error！");
		exit(1);
	}
	
	bzero(&send->addr, sizeof(send->addr));
	
	//设置服务器ip、port
	send->addr.sin_family=AF_INET;
    send->addr.sin_addr.s_addr = inet_addr((char*)send->ipstr);
    send->addr.sin_port = htons(send->port);
	
	printf("sockfd = %d ip = %s  port = %d\n",send->sockfd,send->ipstr,send->port);
}


static void on_net_read(struct ev_loop *loop, struct ev_io *w, int revents)
{
    struct KCPOBJ *send = container_of(w, struct KCPOBJ, ior);
    int ret;
	unsigned int len = sizeof(struct sockaddr_in);

    int recv_size, out_size = 0;
	char recvbuf[4096]={0};

	memset(recvbuf, 0, sizeof(recvbuf));

	recv_size = recvfrom(send->sockfd, recvbuf, 4096, MSG_DONTWAIT,(struct sockaddr *) &send->addr, &len);
	// print_hex("UDP RECV", recvbuf, recv_size);
	if(recv_size < 0)//检测是否有UDP数据包
		return;

	ret = ikcp_input(send->pkcp, recvbuf, recv_size);	
	if(ret < 0)
		return;

	out_size = ikcp_recv(send->pkcp, recvbuf, recv_size);
	if(out_size < 0) // 检测ikcp_recv提取到的数据
		return;
	//如果收到了，则再发其他的包，循环起来
	print_str("IKCP RECV", recvbuf, out_size);
	ikcp_update(send->pkcp,iclock());//不是调用一次两次就起作用，要loop调用
	char bbb[50] = {0};
	memset(bbb, 'z', sizeof(bbb));
	// 发送 zzz
	print_str("IKCP SEND", bbb, sizeof(bbb));
	ikcp_send(send->pkcp, bbb, sizeof(bbb));//strlen(send->buff)+1
	ikcp_update(send->pkcp, iclock());
	ikcp_flush(send->pkcp);
}


static void timeout_cb(EV_P_ ev_timer *w, int revents) {
	struct KCPOBJ *send = container_of(w, struct KCPOBJ, timeout_watcher);
	// 发第一个数据包出去
	char temp[] = "aaaaa\x00";//与服务器初次通信
	print_str("SEND", temp, strlen(temp));
	ikcp_send(send->pkcp, temp, strlen(temp));
	ikcp_update(send->pkcp, iclock());
	ikcp_flush(send->pkcp);
}

int main(int argc,char *argv[])
{
	if(argc != 3)
	{
		printf("usage:\n./client [ip] [port]\n");
		return -1;
	}

	unsigned char *ipstr = (unsigned char *)argv[1];
	unsigned char *port  = (unsigned char *)argv[2];

	// 初始化
	kcpObj send;
	send.ipstr = ipstr;
	send.port = atoi(argv[2]);

	init(&send);//初始化send,主要是设置与服务器通信的套接字对象
	bzero(send.buff,sizeof(send.buff));

	ikcpcb *kcp = ikcp_create(0x11, (void *)&send);//创建kcp对象把send传给kcp的user变量
	kcp->output = udpOutPut;//设置kcp对象的回调函数
	ikcp_nodelay(kcp, 1, 10, 2, 1); //(kcp1, 0, 10, 0, 0); 1, 10, 2, 1
	ikcp_wndsize(kcp, 128, 128);
	send.pkcp = kcp;

	// libev 初始化
	struct ev_loop *loop = EV_DEFAULT;

	// // 用超时来发送第一个数据包
 	// ev_timer_init(&send.timeout_watcher, timeout_cb, 1.5, 1.0);
    // ev_timer_start(loop, &send.timeout_watcher);

	ev_io_init(&send.ior, on_net_read, send.sockfd, EV_READ);
    ev_io_start(loop, &send.ior);

	// 发送第一个数据包，来触发交互
	char temp[] = "aaaaa\x00";//与服务器初次通信
	print_str("SEND", temp, strlen(temp));
	ikcp_send(send.pkcp, temp, strlen(temp));
	ikcp_update(send.pkcp, iclock());
	ikcp_flush(send.pkcp);

	// libev run
	ev_run(loop, 0);
	ev_loop_destroy(loop);
	return 0;
}