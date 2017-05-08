#ifndef _SYS_UTIL_H_
#define _SYS_UTIL_H_

#include "common.h"

/*系统工具模块，目前与系统调用相关的函数还没有在这里封装完毕*/
//根据port建立tcp客户端
int tcp_client(unsigned int port);
//根据主机名或服务器ip地址和端口号启动监听
int tcp_server(const char* host, unsigned short port);


//获取本地IP
int getlocalip(char *ip);

//设置为非阻塞模式
void activate_nonblock(int fd);
//设置为阻塞模式
void deactivate_nonblock(int fd);

//超时函数设置
int read_timeout(int fd, unsigned int wait_seconds);
int write_timeout(int fd, unsigned int wait_seconds);
int accept_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds);
int connect_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds);

//发送和读取的封装
ssize_t readn(int fd, void *buf, size_t count);
ssize_t writen(int fd, const void *buf, size_t count);
ssize_t recv_peek(int sockfd, void *buf, size_t len);
ssize_t readline(int sockfd, void *buf, size_t maxline);

//发送和接收文件描述符
void send_fd(int sock_fd, int fd);
int recv_fd(const int sock_fd);
//根据stat获取日期
const char* statbuf_get_date(struct stat *sbuf);
//根据stat获取权限
const char* statbuf_get_perms(struct stat *sbuf);

//加读锁
int lock_read_file(int fd);
//加写锁
int lock_write_file(int fd);
//解锁
int unlock_file(int fd);

//获取当前时间秒和微秒
long get_curtime_sec(void);
long get_curtime_usec(void);

//睡眠time_sleep秒
void nano_sleep(double time_sleep);

//接收oob信号
void active_oobinline(int fd);

//sigurg信号
void active_sigurg(int fd);


#endif /* _SYS_UTIL_H_ */

