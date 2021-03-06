#include "sysutil.h"

// 返回sock, port=0 不绑定端口
int tcp_client(unsigned int port)
{
	int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        ERR_EXIT("socket error");
	
	if (port > 0)
	{
		struct sockaddr_in localaddr;
		memset(&localaddr, 0, sizeof(localaddr));
		localaddr.sin_family = AF_INET;
		localaddr.sin_port = htons(port);
		char ip[20]= {0};
		getlocalip(ip);
		localaddr.sin_addr.s_addr = inet_addr(ip);

		int on = 1;
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
			ERR_EXIT("setsockopt");
		
		if(bind(sock, (struct sockaddr*)&localaddr, sizeof(localaddr)) == -1)
			ERR_EXIT("bind");
	}

	return sock;
}

/*
* tcp_server-启动tcp服务器
* @host:服务器的Ip地址或者服务器主机名
* @port:服务器端口号
* 成功返回监听套接字，失败返回-1
*/
int tcp_server(const char* host, unsigned short port)
{
	int listenfd;
	if ((listenfd = socket(AF_INET,SOCK_STREAM, 0)) < 0)
		ERR_EXIT("tcp_server");
	
	struct sockaddr_in servaddr;
	memset(&servaddr, 0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if (host != NULL)
	{
		//根据主机名找到ip地址
		if (inet_aton(host, &servaddr.sin_addr) == 0)
		{
			struct hostent *hp;
			hp = gethostbyname(host);
			if (hp == NULL)
				ERR_EXIT("gethostbyname");

			servaddr.sin_addr = *(struct in_addr*)hp->h_addr;
		}
	}
	else
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	servaddr.sin_port = htons(port);
	
	//设置地址重复利用
	int on = 1;
	if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on))) < 0)
		ERR_EXIT("setsockopt");

	if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		ERR_EXIT("bind");

	if (listen(listenfd, SOMAXCONN) < 0)
		ERR_EXIT("listen");

	return listenfd;
}

//获取本地ip
/*
int getlocalip(char *ip)
{
	char host[100] = {0};
	if (gethostname(host, sizeof(host)) < 0)
		return -1;
	struct hostent *hp;
	if ((hp = gethostbyname(host)) == NULL)
	return -1;

	strcpy(ip, inet_ntoa(*(struct in_addr*)hp->h_addr));
	return 0;
}
*/
int getlocalip(char *ipaddr)
{
    int sock_get_ip; 
  
    struct   sockaddr_in *sin;  
    struct   ifreq ifr_ip;     
  
    if ((sock_get_ip=socket(AF_INET, SOCK_STREAM, 0)) == -1)  
    {  
         printf("socket create fail...GetLocalIp!/n");  
         return 0;  
    }  
     
    memset(&ifr_ip, 0, sizeof(ifr_ip));     
    strncpy(ifr_ip.ifr_name, "eth0", sizeof(ifr_ip.ifr_name) - 1);     
   
    if( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) < 0 )     
    {     
         return 0;     
    }       
    sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;     
    strcpy(ipaddr,inet_ntoa(sin->sin_addr));         
   
    close( sock_get_ip ); 
	
    return 1;
}

/**
 * activate_noblock - 设置I/O为非阻塞模式
 * @fd: 文件描符符
 */
void activate_nonblock(int fd)
{
	int ret;
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		ERR_EXIT("fcntl");

	flags |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);
	if (ret == -1)
		ERR_EXIT("fcntl");
}

/**
 * deactivate_nonblock - 设置I/O为阻塞模式
 * @fd: 文件描符符
 */
void deactivate_nonblock(int fd)
{
	int ret;
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		ERR_EXIT("fcntl");

	flags &= ~O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, flags);
	if (ret == -1)
		ERR_EXIT("fcntl");
}

/**
 * read_timeout - 读超时检测函数，不含读操作
 * @fd: 文件描述符
 * @wait_seconds: 等待超时秒数，如果为0表示不检测超时
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int read_timeout(int fd, unsigned int wait_seconds)
{
	int ret = 0;
	if (wait_seconds > 0)
	{
		fd_set read_fdset;
		struct timeval timeout;

		FD_ZERO(&read_fdset);
		FD_SET(fd, &read_fdset);

		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			////select 函数会阻塞直到检测到事件或者超时
			//超时返回0，事件到来返回到来的事件数目(这里只有一个可读的事件)
			ret = select(fd + 1, &read_fdset, NULL, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);

		if (ret == 0)
		{
			ret = -1;
			errno = ETIMEDOUT;
		}
		else if (ret == 1)
			ret = 0;
	}

	return ret;
}

/**
 * write_timeout - 写超时检测函数，不含写操作
 * @fd: 文件描述符
 * @wait_seconds: 等待超时秒数，如果为0表示不检测超时
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int write_timeout(int fd, unsigned int wait_seconds)
{
	int ret = 0;
	if (wait_seconds > 0)
	{
		fd_set write_fdset;
		struct timeval timeout;

		FD_ZERO(&write_fdset);
		FD_SET(fd, &write_fdset);

		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			ret = select(fd + 1, NULL, NULL, &write_fdset, &timeout);
		} while (ret < 0 && errno == EINTR);

		if (ret == 0)
		{
			ret = -1;
			errno = ETIMEDOUT;
		}
		else if (ret == 1)
			ret = 0;
	}

	return ret;
}

/**
 * accept_timeout - 带超时的accept
 * @fd: 套接字
 * @addr: 输出参数，返回对方地址
 * @wait_seconds: 等待超时秒数，如果为0表示正常模式
 * 成功（未超时）返回已连接套接字，超时返回-1并且errno = ETIMEDOUT
 */
int accept_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
	int ret;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if (wait_seconds > 0)
	{
		fd_set accept_fdset;
		struct timeval timeout;
		FD_ZERO(&accept_fdset);
		FD_SET(fd, &accept_fdset);
		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			ret = select(fd + 1, &accept_fdset, NULL, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);
		if (ret == -1)
			return -1;
		else if (ret == 0)
		{
			errno = ETIMEDOUT;
			return -1;
		}
	}
	//没有超时，接受连接
	if (addr != NULL)
		ret = accept(fd, (struct sockaddr*)addr, &addrlen);
	else
		ret = accept(fd, NULL, NULL);
/*	if (ret == -1)
		ERR_EXIT("accept");
		*/

	return ret;
}

/**
 * connect_timeout - connect
 * @fd: 套接字
 * @addr: 要连接的对方地址
 * @wait_seconds: 等待超时秒数，如果为0表示正常模式
 * 成功（未超时）返回0，失败返回-1，超时返回-1并且errno = ETIMEDOUT
 */
int connect_timeout(int fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
	int ret;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if (wait_seconds > 0)
		activate_nonblock(fd);

	ret = connect(fd, (struct sockaddr*)addr, addrlen);
	if (ret < 0 && errno == EINPROGRESS)
	{
		fd_set connect_fdset;
		struct timeval timeout;
		FD_ZERO(&connect_fdset);
		FD_SET(fd, &connect_fdset);
		timeout.tv_sec = wait_seconds;
		timeout.tv_usec = 0;
		do
		{
			/* 一旦连接建立，套接字就可写 */
			ret = select(fd + 1, NULL, &connect_fdset, NULL, &timeout);
		} while (ret < 0 && errno == EINTR);
		if (ret == 0)
		{
			ret = -1;
			errno = ETIMEDOUT;
		}
		else if (ret < 0)
			return -1;
		else if (ret == 1)
		{
			/* ret返回为1，可能有两种情况，一种是连接建立成功，一种是套接字产生错误，*/
			/* 此时错误信息不会保存至errno变量中，因此，需要调用getsockopt来获取。 */
			int err;
			socklen_t socklen = sizeof(err);
			int sockoptret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &socklen);
			if (sockoptret == -1)
			{
				return -1;
			}
			if (err == 0)
			{
				ret = 0;
			}
			else
			{
				errno = err;
				ret = -1;
			}
		}
	}
	if (wait_seconds > 0)
	{
		deactivate_nonblock(fd);
	}
	return ret;
}

/**
 * readn - 读取固定字节数
 * @fd: 文件描述符
 * @buf: 接收缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1，读到EOF返回<count
 */
ssize_t readn(int fd, void *buf, size_t count)
{
	size_t nleft = count;
	ssize_t nread;
	char *bufp = (char*)buf;

	while (nleft > 0)
	{
		if ((nread = read(fd, bufp, nleft)) < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		else if (nread == 0)
			return count - nleft;

		bufp += nread;
		nleft -= nread;
	}

	return count;
}

/**
 * writen - 发送固定字节数
 * @fd: 文件描述符
 * @buf: 发送缓冲区
 * @count: 要读取的字节数
 * 成功返回count，失败返回-1
 */
ssize_t writen(int fd, const void *buf, size_t count)
{
	size_t nleft = count;
	ssize_t nwritten;
	char *bufp = (char*)buf;

	while (nleft > 0)
	{
		if ((nwritten = write(fd, bufp, nleft)) < 0)
		{
			if (errno == EINTR)
				continue;
			return -1;
		}
		else if (nwritten == 0)
			continue;

		bufp += nwritten;
		nleft -= nwritten;
	}

	return count;
}

/**
 * recv_peek - 仅仅查看套接字缓冲区数据，但不移除数据
 * @sockfd: 套接字
 * @buf: 接收缓冲区
 * @len: 长度
 * 成功返回>=0，失败返回-1
 */
ssize_t recv_peek(int sockfd, void *buf, size_t len)
{
	while (1)
	{
		int ret = recv(sockfd, buf, len, MSG_PEEK);
		if (ret == -1 && errno == EINTR)
			continue;
		return ret;
	}
}

/**
 * readline - 按行读取数据
 * @sockfd: 套接字
 * @buf: 接收缓冲区
 * @maxline: 每行最大长度
 * 成功返回>=0，失败返回-1
 */
ssize_t readline(int sockfd, void *buf, size_t maxline)
{
	int ret;
	int nread;
	char *bufp = buf;
	int nleft = maxline;
	while (1)
	{
		ret = recv_peek(sockfd, bufp, nleft);
		if (ret < 0)
			return ret;
		else if (ret == 0)
			return ret;

		nread = ret;
		int i;
		for (i=0; i<nread; i++)
		{
			if (bufp[i] == '\n')
			{
				ret = readn(sockfd, bufp, i+1);//使用readn把数据读走
				if (ret != i+1)
					exit(EXIT_FAILURE);

				return ret;
			}
		}

		if (nread > nleft)
			exit(EXIT_FAILURE);

		nleft -= nread;
		ret = readn(sockfd, bufp, nread);
		if (ret != nread)
			exit(EXIT_FAILURE);

		bufp += nread;
	}

	return -1;
}

//发送文件描述符：通过msghdr结构体发送
void send_fd(int sock_fd, int fd)
{
	int ret;
	struct msghdr msg;
	struct cmsghdr *p_cmsg;
	struct iovec vec;
	char cmsgbuf[CMSG_SPACE(sizeof(fd))];
	int *p_fds;
	char sendchar = 0;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	p_cmsg = CMSG_FIRSTHDR(&msg);//返回一个指向附属数据缓冲区的第一个附属对象的struct cmsghdr指针
	p_cmsg->cmsg_level = SOL_SOCKET;
	p_cmsg->cmsg_type = SCM_RIGHTS;
	p_cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	//返回：跟在头部以及填充字节后的附属数据的第一个字节（如果存在）的地址。
	p_fds = (int*)CMSG_DATA(p_cmsg);
	*p_fds = fd;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;

	vec.iov_base = &sendchar;
	vec.iov_len = sizeof(sendchar);
	ret = sendmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("sendmsg");
}

int recv_fd(const int sock_fd)
{
	int ret;
	struct msghdr msg;
	char recvchar;
	struct iovec vec;
	int recv_fd;
	char cmsgbuf[CMSG_SPACE(sizeof(recv_fd))];
	struct cmsghdr *p_cmsg;
	int *p_fd;
	vec.iov_base = &recvchar;
	vec.iov_len = sizeof(recvchar);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	msg.msg_flags = 0;

	p_fd = (int*)CMSG_DATA(CMSG_FIRSTHDR(&msg));
	*p_fd = -1;  
	ret = recvmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("recvmsg");

	p_cmsg = CMSG_FIRSTHDR(&msg);
	if (p_cmsg == NULL)
		ERR_EXIT("no passed fd");


	p_fd = (int*)CMSG_DATA(p_cmsg);
	recv_fd = *p_fd;
	if (recv_fd == -1)
		ERR_EXIT("no passed fd");

	return recv_fd;
}

//获取时间
const char* statbuf_get_date(struct stat *sbuf)
{
	static char datebuf[64] = {0};
	const char *p_date_format = "%b %e %H:%M";
	struct timeval tv;
	gettimeofday(&tv, NULL);//获取当前
	time_t local_time = tv.tv_sec;
	//超过半年就不显示小时和分钟，而是显示年份
	if (sbuf->st_mtime > local_time || (local_time - sbuf->st_mtime) > 60*60*24*182)
	{
		p_date_format = "%b %e  %Y";
	}

	struct tm* p_tm = localtime(&sbuf->st_mtime);//根据秒数，转为本地时间
	strftime(datebuf, sizeof(datebuf), p_date_format, p_tm);//格式化到datebuf中

	return datebuf;
}

//获取权限位
const char* statbuf_get_perms(struct stat *sbuf)
{
	static char perms[] = "----------";
	perms[0] = '?';

	mode_t mode = sbuf->st_mode;
	switch (mode & S_IFMT)
	{
	case S_IFREG:
		perms[0] = '-';
		break;
	case S_IFDIR:
		perms[0] = 'd';
		break;
	case S_IFLNK:
		perms[0] = 'l';
		break;
	case S_IFIFO:
		perms[0] = 'p';
		break;
	case S_IFSOCK:
		perms[0] = 's';
		break;
	case S_IFCHR:
		perms[0] = 'c';
		break;
	case S_IFBLK:
		perms[0] = 'b';
		break;
	}

	if (mode & S_IRUSR)
	{
		perms[1] = 'r';
	}
	if (mode & S_IWUSR)
	{
		perms[2] = 'w';
	}
	if (mode & S_IXUSR)
	{
		perms[3] = 'x';
	}
	if (mode & S_IRGRP)
	{
		perms[4] = 'r';
	}
	if (mode & S_IWGRP)
	{
		perms[5] = 'w';
	}
	if (mode & S_IXGRP)
	{
		perms[6] = 'x';
	}
	if (mode & S_IROTH)
	{
		perms[7] = 'r';
	}
	if (mode & S_IWOTH)
	{
		perms[8] = 'w';
	}
	if (mode & S_IXOTH)
	{
		perms[9] = 'x';
	}
	if (mode & S_ISUID)
	{
		perms[3] = (perms[3] == 'x') ? 's' : 'S';
	}
	if (mode & S_ISGID)
	{
		perms[6] = (perms[6] == 'x') ? 's' : 'S';
	}
	if (mode & S_ISVTX)
	{
		perms[9] = (perms[9] == 'x') ? 't' : 'T';
	}

	return perms;
}

/*
           struct flock {
               ...
               short l_type;    //Type of lock: F_RDLCK,
                                   F_WRLCK, F_UNLCK 
               short l_whence;  // How to interpret l_start:
                                   SEEK_SET, SEEK_CUR, SEEK_END 
               off_t l_start;   // Starting offset for lock 
               off_t l_len;     // Number of bytes to lock 
               pid_t l_pid;     // PID of process blocking our lock
                                   (F_GETLK only) 
               ...
           };

*/
//对fd加读锁
int lock_read_file(int fd)
{
	struct flock lockfile;
	memset(&lockfile, 0, sizeof(lockfile));
	int ret;
	lockfile.l_type = F_RDLCK;
	lockfile.l_whence = SEEK_SET;
	lockfile.l_start = 0;
	lockfile.l_len = 0;
	
	do
	{
		ret = fcntl(fd, F_SETLKW, &lockfile); //阻塞
	}
	while (ret == -1 && errno == EINTR);
	
	return ret;
}
//加写锁
int lock_write_file(int fd)
{
	struct flock lockfile;
	memset(&lockfile, 0, sizeof(lockfile));
	int ret;
	lockfile.l_type = F_WRLCK;
	lockfile.l_whence = SEEK_SET;
	lockfile.l_start = 0;
	lockfile.l_len = 0;
	
	do
	{
		ret = fcntl(fd, F_SETLKW, &lockfile); //阻塞
	}
	while (ret == -1 && errno == EINTR);
	
	return ret;
}
//解锁
int unlock_file(int fd)
{
	struct flock lockfile;
	memset(&lockfile, 0, sizeof(lockfile));
	int ret;
	lockfile.l_type = F_UNLCK;
	lockfile.l_whence = SEEK_SET;
	lockfile.l_start = 0;
	lockfile.l_len = 0;

	ret = fcntl(fd, F_SETLK, &lockfile); //不阻塞
	return ret;
}

//静态变量
static struct timeval curtime;

long get_curtime_sec(void)
{
	if (gettimeofday(&curtime, NULL) < 0)
	{
		ERR_EXIT("gettimeofday");
	}

	return curtime.tv_sec;
}

long get_curtime_usec(void)
{
	return curtime.tv_usec;
}

//睡眠
void nano_sleep(double time_sleep)
{
	time_t tsec = (time_t)time_sleep;
	double fractional = time_sleep - (double)tsec;

	struct timespec timesp;
	timesp.tv_sec = tsec;
	timesp.tv_nsec = (long)(fractional * (double)1000000000);
	
	int ret;
	do
	{
		ret = nanosleep(&timesp, &timesp);
	}
	while (ret == -1 && errno == EINTR);

}

//开启fd能够接收带外数据的功能
void active_oobinline(int fd)
{
	int ret;
	int oobinline = 1;
	ret = setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &oobinline, sizeof(oobinline));
	if (ret == -1)
	{
		ERR_EXIT("setsockopt");
	}
}

// 当fd 产生带外数据时会产生SIGURG信号
// 函数开启当前进程接收SIGURG信号的功能
// 在信号处理函数中去接收带外数据
void active_sigurg(int fd)
{
	int ret;
	ret = fcntl(fd, F_SETOWN, getpid());
	if (ret == -1)
	{
		ERR_EXIT("fcntl");
	}
}
