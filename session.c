#include "session.h"
#include "common.h"
#include "ftpproto.h"
#include "sysutil.h"
#include "privparent.h"
#include "privsock.h"


void begin_session(session_t *sess)
{
	//��ʼ��
	priv_sock_init(sess);
	
	
	pid_t pid;
	pid = fork();
	if (pid < 0)
		ERR_EXIT("fork");
	if (pid == 0)
	{
		//ftpЭ���������
		priv_sock_set_child_context(sess);
		handle_child(sess);//�����ӽ��̣���ftp������������ɵ�����

	}
	else
	{
		//nobody����
		priv_sock_set_parent_context(sess);
		handle_parent(sess);//�������̣���nobody������ɵ�����
	}
}