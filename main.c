#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "parseconf.h"
#include "tunable.h"
#include "ftpproto.h"
#include "ftpcodes.h"

extern session_t* p_sess;
static unsigned int cur_childrens;
void check_clients_limit(session_t* sess);
void handle_sigchld(int);

int main()
{
	
	parseconf_load_file(MINIFTPD_CONF);
	
	printf("pasv_enable = %d\n", tunable_pasv_enable);
	printf("port_enable = %d\n", tunable_port_enable);
	printf("listen_port = %d\n", tunable_listen_port);
	printf("max_per_ip = %d\n", tunable_max_per_ip);
	printf("accept_timeout = %d\n", tunable_accept_timeout);
	printf("idle_session_timeout = %d\n", tunable_idle_session_timeout);
	printf("data_connection_timeout = %d\n", tunable_data_connection_timeout);
	printf("local_umask = %d\n", tunable_local_umask);
	printf("upload_max_rate = %d\n", tunable_upload_max_rate);
	printf("download_max_rate = %d\n", tunable_download_max_rate);
	
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
	
	p_sess = &sess;
	
	//初始化最大速率
	sess.upload_speed_max = tunable_upload_max_rate;
	sess.download_speed_max = tunable_download_max_rate;
	signal(SIGCHLD, handle_sigchld);//捕捉子进程退出的信号
	
    int listenfd = tcp_server(NULL, 8888);
	int conn;
	while(1)
	{
		conn = accept_timeout(listenfd, NULL, 0);
		if (conn == -1)
			ERR_EXIT("accpet_timeout");
	
		++cur_childrens;
		sess.num_clients = cur_childrens;

		pid_t pid = fork();
		if (pid == -1)
			ERR_EXIT("fork");
		if (pid == 0)//子进程
		{
			close(listenfd);
			//抽象成一个会话
			sess.ctrl_fd = conn;
			check_clients_limit(&sess);//连接数的限制
			begin_session(&sess);
		}
		else
			close(conn);
		
	}
	return 0;
}

void check_clients_limit(session_t* sess)
{
	if (tunable_max_clients > 0 && sess->num_clients > tunable_max_clients)
	{
		ftp_reply(sess, FTP_TOO_MANY_USERS, "There are too many connected users, Please try later.");
		exit(EXIT_FAILURE);
	}

}

void handle_sigchld(int sig)
{
	pid_t pid;
	// 如果多个子进程同时断开连接
	while ((pid = waitpid(-1 ,NULL, WNOHANG)) > 0)
	{
		;
	}
	--cur_childrens;
	
}
