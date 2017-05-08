#ifndef _SESSION_H_
#define _SESSION_H_
#include "common.h"

typedef struct session
{
	//��������
	uid_t uid; //uid
	int ctrl_fd;//��������ͨ����fd
	char cmdline[MAX_COMMAND_LINE];//������
	char cmd[MAX_COMMAND];//����
	char arg[MAX_ARG];//����

	// ftpЭ�������nobody����ͨ��
	int ftp_fd;		//ftpЭ����̵�fd
	int nobody_fd;	//nobody���̵�fd

	//����
	unsigned int upload_speed_max;		//����ϴ��ٶ�
	unsigned int download_speed_max;	//��������ٶ�
	long transfer_start_sec;			//��ʼ����ʱ����
	long transfer_start_usec;			//��ʼ����ʱ��΢��

	//��������
	struct sockaddr_in* p_sock;			//�������ӵ�ip
	int data_fd;						//�������ӵ�fd
	int pasv_listen_fd;					//pasvģʽ������fd
	int data_process;					//�Ƿ������ݴ���

	//ftpЭ�����
	int is_ascii;				//�Ƿ���asciiģʽ
	long long restart_pos;		//�ش���ʼ��λ��
	char* rnfr_name;			//rnfr������
	int abor_received;			//�Ƿ����abor

	//�ͻ�������������
	int num_clients;		//�ͻ�����Ŀ
	int num_this_ip;		//��ip����Ŀ

} session_t;

void begin_session(session_t *sess);

#endif