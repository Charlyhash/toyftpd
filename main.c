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
		//��������
		0, -1, "", "", "",

		// ftpЭ�������nobody����ͨ��
		-1, -1,
		
		//����
		0, 0, 0, 0,
		// ��������
		NULL, -1, -1, 0,

		// ftpЭ�����
		0, 0, NULL, 0,

		//�ͻ�������������
		0, 0
	};
	
	p_sess = &sess;
	
	//��ʼ���������
	sess.upload_speed_max = tunable_upload_max_rate;
	sess.download_speed_max = tunable_download_max_rate;
	signal(SIGCHLD, handle_sigchld);//��׽�ӽ����˳����ź�
	
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
		if (pid == 0)//�ӽ���
		{
			close(listenfd);
			//�����һ���Ự
			sess.ctrl_fd = conn;
			check_clients_limit(&sess);//������������
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
	// �������ӽ���ͬʱ�Ͽ�����
	while ((pid = waitpid(-1 ,NULL, WNOHANG)) > 0)
	{
		;
	}
	--cur_childrens;
	
}
