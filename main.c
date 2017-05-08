#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "parseconf.h"


int main()
{
	
	parseconf_load_file(MINIFTPD_CONF);
	
	if(getuid() != 0)
	{
		fprintf(stderr, "miniftpd must be started as root\n");
		exit(EXIT_FAILURE);
	}
    else
        printf("miniftpd open!\n");
	session_t sess =
	{
		//控制连接
		0, -1, "", "", "",

		// ftp协议进程与nobody进程通信
		-1, -1,
		
		//限速
		0, 0, 0, 0,
		// 数据连接
		NULL, -1, -1, 0,

		// ftp协议控制
		0, 0, NULL, 0,

		//客户端连接数控制
		0, 0
	};
    int listenfd = tcp_server(NULL, 8888);
	int conn;
	while(1)
	{
		conn = accept_timeout(listenfd, NULL, 0);
		if (conn == -1)
			ERR_EXIT("accpet_timeout");
		pid_t pid = fork();
		if (pid == -1)
			ERR_EXIT("fork");
		if (pid == 0)//子进程
		{
			close(listenfd);
			//抽象成一个会话
			sess.ctrl_fd = conn;
			begin_session(&sess);
		}
		else
			close(conn);
		
	}
	return 0;
}
