#ifndef _SESSION_H_
#define _SESSION_H_
#include "common.h"

typedef struct session
{
	//控制连接
	uid_t uid; //uid
	int ctrl_fd;//控制连接通道的fd
	char cmdline[MAX_COMMAND_LINE];//命令行
	char cmd[MAX_COMMAND];//命令
	char arg[MAX_ARG];//参数

	// ftp协议进程与nobody进程通信
	int ftp_fd;		//ftp协议进程的fd
	int nobody_fd;	//nobody进程的fd

	//限速
	unsigned int upload_speed_max;		//最大上传速度
	unsigned int download_speed_max;	//最大下载速度
	long transfer_start_sec;			//开始传输时间秒
	long transfer_start_usec;			//开始传输时间微秒

	//数据连接
	struct sockaddr_in* p_sock;			//数据连接的ip
	int data_fd;						//数据连接的fd
	int pasv_listen_fd;					//pasv模式监听的fd
	int data_process;					//是否处于数据处理

	//ftp协议控制
	int is_ascii;				//是否是ascii模式
	long long restart_pos;		//重传开始的位置
	char* rnfr_name;			//rnfr的名字
	int abor_received;			//是否接受abor

	//客户端连接数控制
	int num_clients;		//客户端数目
	int num_this_ip;		//本ip的数目

} session_t;

void begin_session(session_t *sess);

#endif