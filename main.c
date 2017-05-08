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
		if (pid == 0)//�ӽ���
		{
			close(listenfd);
			//�����һ���Ự
			sess.ctrl_fd = conn;
			begin_session(&sess);
		}
		else
			close(conn);
		
	}
	return 0;
}
