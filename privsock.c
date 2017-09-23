#include "privsock.h"
#include "sysutil.h"

//初始化，使用socketpair创建进程间通信管道(unix域套接字)
//ftp服务进程和nobody进程通过该管道通信
void priv_sock_init(session_t *sess)
{
	int sockfds[2];
	if ((socketpair(PF_UNIX, SOCK_STREAM, 0, sockfds)) < 0)
		ERR_EXIT("socketpair");

	sess->ftp_fd = sockfds[1];
	sess->nobody_fd = sockfds[0];
}

//关闭管道
void priv_sock_close(session_t *sess)
{
	if (sess->ftp_fd != -1)
	{
		close(sess->ftp_fd);
		sess->ftp_fd = -1;
	}

	if (sess->nobody_fd != -1)
	{
		close(sess->nobody_fd);
		sess->nobody_fd = -1;
	}
}

//设置父进程的上下文：关闭fpt服务进程的fd
void priv_sock_set_parent_context(session_t *sess)
{
	if (sess->ftp_fd != -1)
	{
		close(sess->ftp_fd);
		sess->ftp_fd = -1;
	}
}
//设置子进程的上下文：关闭nobody进程fd
void priv_sock_set_child_context(session_t *sess)
{
	if (sess->nobody_fd != -1)
	{
		close(sess->nobody_fd);
		sess->nobody_fd = -1;
	}		
}

//发送命令（子->父），命令在头文件中以宏的形式定义为1，2，3，4这四种
//发送一个char就可以了
void priv_sock_send_cmd(int fd, char cmd)
{
	int ret = writen(fd, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
	{
		fprintf(stderr, "priv_sock_send_cmd error.");
		exit(EXIT_FAILURE);
	}
}

//接收命令（父<-子）
char priv_sock_get_cmd(int fd)
{
	char res;
	int ret = readn(fd, &res, sizeof(res));

	if (ret == 0)
	{
		printf("ftpproto process exit\n");
		exit(EXIT_SUCCESS);
	}

	if (ret != sizeof(res))
	{
		fprintf(stderr, "priv_sock_get_cmd error.");
		exit(EXIT_FAILURE);
	}

	return res;
}

//发送应答的结果(nobody进程对FTP服务进程的应答)
//为1，2
void priv_sock_send_result(int fd, char res)
{
	int ret = writen(fd, &res, sizeof(res));
	if (ret != sizeof(res))
	{
		fprintf(stderr, "priv_sock_send_result error.");
		exit(EXIT_FAILURE);
	}
}

//接收应答的结果
char priv_sock_get_result(int fd)
{
	char res;
	int ret = readn(fd, &res, sizeof(res));
	if (ret != sizeof(res))
	{
		fprintf(stderr, "priv_sock_get_result error.");
		exit(EXIT_FAILURE);
	}

	return res;
}

//发送int给fd
void priv_sock_send_int(int fd, int the_int)
{
	//写入
	int ret = writen(fd, &the_int, sizeof(the_int));
	if (ret != sizeof(the_int))
	{
		fprintf(stderr, "priv_sock_send_int error.");
		exit(EXIT_FAILURE);
	}
}

//从fd上接收int
int priv_sock_get_int(int fd)
{
	int res;
	int ret = readn(fd, &res, sizeof(res));//阻塞，直到对方writen调用完毕
	if (ret != sizeof(res))
	{
		fprintf(stderr, "priv_sock_get_int error.");
		exit(EXIT_FAILURE);
	}

	return res;
}

//发送字符串，要发送的字符串和长度
void priv_sock_send_buf(int fd, const char *buf, unsigned int len)
{
	//先发送int，再发送字符串
	priv_sock_send_int(fd, len);
	int ret;
	ret = writen(fd, buf, len);
	if (ret != (int)len)
	{
		fprintf(stderr, "priv_sock_send_buf error.");
		exit(EXIT_FAILURE);
	}
}

//接收一个字符串
void priv_sock_recv_buf(int fd, char *buf, unsigned int len)
{
	//先接收int，再接收字符串
	unsigned int recv_len = (unsigned int)priv_sock_get_int(fd);
	if (recv_len > len)
	{
		fprintf(stderr, "priv_sock_recv_buf error.");
		exit(EXIT_FAILURE);
	}

	int ret;
	ret = readn(fd, buf, recv_len);
	if (ret != (int)recv_len)
	{
		fprintf(stderr, "priv_sock_recv_buf error.");
		exit(EXIT_FAILURE);
	}
}

//发送文件描述符
void priv_sock_send_fd(int sock_fd, int fd)
{
	send_fd(sock_fd, fd);
}

//接收文件描述符
int priv_sock_recv_fd(int sock_fd)
{
	return recv_fd(sock_fd);
}

