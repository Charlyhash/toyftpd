#include "session.h"
#include "common.h"
#include "ftpproto.h"
#include "sysutil.h"
#include "privparent.h"
#include "privsock.h"


void begin_session(session_t *sess)
{
	//初始化
	priv_sock_init(sess);
	
	
	pid_t pid;
	pid = fork();
	if (pid < 0)
		ERR_EXIT("fork");
	if (pid == 0)
	{
		//ftp协议解析进程
		priv_sock_set_child_context(sess);
		handle_child(sess);//处理子进程，即ftp服务器进程完成的任务

	}
	else
	{
		//nobody进程
		priv_sock_set_parent_context(sess);
		handle_parent(sess);//处理父进程，即nobody进程完成的任务
	}
}