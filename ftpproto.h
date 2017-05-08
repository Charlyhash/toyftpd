#ifndef _FTP_PROTO_H_
#define _FTP_PROTO_H_

#include "session.h"


typedef void (*CMD_HANDLER)(session_t *sess);
//ftp命令的结构体：由命令字符串组和对应的处理函数组成
typedef struct ftpcmd
{
	const char *cmd;
	CMD_HANDLER cmd_handler;
	
} ftpcmd_t;

//ftp服务进程
void handle_child(session_t *sess);

//ftp应答：传递状态和文本，返回给客户端
void ftp_reply(session_t* sess, int status, const char* text);
//ftp应答：有‘-’
void ftp_lreply(session_t* sess, int status, const char* text);

//得到传输的文件描述符
int get_transfer_fd(session_t*);
//得到port模式的fd
int get_port_fd(session_t* sess);
//得到pasv模式的fd
int get_pasv_fd(session_t* sess);

//返回llist给客户端
void list_common(session_t* sess, int detail);
//下载
void upload_common(session_t* sess, int is_append);



#endif 