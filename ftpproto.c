#include "ftpproto.h"
#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "ftpcodes.h"
#include "tunable.h"
#include "privparent.h"
#include "privsock.h"
#include "str.h"

//一些命令映射函数
static void do_user(session_t *sess);
static void do_pass(session_t *sess);
static void do_cwd(session_t *sess);
static void do_cdup(session_t *sess);
static void do_quit(session_t *sess);
static void do_port(session_t *sess);
static void do_pasv(session_t *sess);
static void do_type(session_t *sess);
static void do_retr(session_t *sess);
static void do_stor(session_t *sess);
static void do_appe(session_t *sess);
static void do_list(session_t *sess);
static void do_nlst(session_t *sess);
static void do_rest(session_t *sess);
static void do_abor(session_t *sess);
static void do_pwd(session_t *sess);
static void do_mkd(session_t *sess);
static void do_rmd(session_t *sess);
static void do_dele(session_t *sess);
static void do_rnfr(session_t *sess);
static void do_rnto(session_t *sess);
static void do_site(session_t *sess);
static void do_syst(session_t *sess);
static void do_feat(session_t *sess);
static void do_size(session_t *sess);
static void do_stat(session_t *sess);
static void do_noop(session_t *sess);
static void do_help(session_t *sess);

static ftpcmd_t ctrl_cmds[] = {
	/* 访问控制命令 */
	{"USER",	do_user	},
	{"PASS",	do_pass	},
	{"CWD",		do_cwd	},
	{"XCWD",	do_cwd	},
	{"CDUP",	do_cdup	},
	{"XCUP",	do_cdup	},
	{"QUIT",	do_quit	},
	{"ACCT",	NULL	},
	{"SMNT",	NULL	},
	{"REIN",	NULL	},

	/* 传输参数命令 */
	{"PORT",	do_port	},
	{"PASV",	do_pasv	},
	{"TYPE",	do_type	},
	{"STRU",	NULL	},
	{"MODE",	NULL	},

	/* 服务命令 */
	{"RETR",	do_retr	},
	{"STOR",	do_stor	},
	{"APPE",	do_appe	},
	{"LIST",	do_list	},
	{"NLST",	do_nlst	},
	{"REST",	do_rest	},
	{"ABOR",	do_abor	},
	{"\377\364\377\362ABOR", do_abor},
	{"PWD",		do_pwd	},
	{"XPWD",	do_pwd	},
	{"MKD",		do_mkd	},
	{"XMKD",	do_mkd	},
	{"RMD",		do_rmd	},
	{"XRMD",	do_rmd	},
	{"DELE",	do_dele	},
	{"RNFR",	do_rnfr	},
	{"RNTO",	do_rnto	},
	{"SITE",	do_site	},
	{"SYST",	do_syst	},
	{"FEAT",	do_feat },
	{"SIZE",	do_size	},
	{"STAT",	do_stat	},
	{"NOOP",	do_noop	},
	{"HELP",	do_help	},
	{"STOU",	NULL	},
	{"ALLO",	NULL	},
	{ NULL,		NULL	}
};

session_t* p_sess;

void start_data_alarm();

//数据连接通道超时闹钟
void handle_data_alarm(int sig)
{
	if (!p_sess->data_process)
	{
		ftp_reply(p_sess, FTP_DATA_TIMEOUT, "Data timeout. Reconnect. Sorry.");
		exit(EXIT_FAILURE);
	}
	
	//如果此时正处在数据传输过程中
	start_data_alarm(); //重新开启数据通道闹钟
}
//开启数据通道闹钟
void start_data_alarm(void)
{
	if (tunable_data_connection_timeout > 0)
	{
		//安装信号 
		signal(SIGALRM, handle_data_alarm);
		//开启闹钟 (如果此前有安装过闹钟，会取消之前的）
		alarm(tunable_data_connection_timeout);
	}
	else if (tunable_idle_session_timeout > 0)
	{
		alarm(0); //取消控制通道的闹钟
	}
}

//闹钟处理函数
void handle_alarm(int sig)
{
	//关闭读的这一半
	shutdown(p_sess->ctrl_fd, SHUT_RD);
	ftp_reply(p_sess, FTP_IDLE_TIMEOUT, "Timeout.");
	shutdown(p_sess->ctrl_fd, SHUT_WR);
	exit(EXIT_FAILURE);//退出
}

//开启控制通道闹钟
void start_cmd_alarm()
{
	if (tunable_idle_session_timeout > 0)
	{
		//安装信号
		signal(SIGALRM, handle_alarm);
		//开启闹钟
		alarm(tunable_idle_session_timeout);
	}
}

void check_abor(session_t* sess)
{
	if (sess->abor_received)
	{
		ftp_reply(p_sess, FTP_ABOROK, "ABOR successful.");
	}
	sess->abor_received = 0;
}

//ftp服务进程
void handle_child(session_t *sess)
{
	//发送信息给客户端
	ftp_reply(sess, FTP_GREET, "toyftpd version 0.1");
	int ret;
	//从客户端一行一行接收数据
	while(1)
	{
		memset(sess->cmdline, 0, sizeof(sess->cmdline));
		memset(sess->cmd,0, sizeof(sess->cmd));
		memset(sess->arg,0, sizeof(sess->arg));
		ret = readline(sess->ctrl_fd, sess->cmdline, MAX_COMMAND_LINE);
		if (ret == -1)
			ERR_EXIT("readline");
		else if (ret == 0)
			exit(0);

		start_cmd_alarm(); //开启控制通道闹钟
		
		//解析ftp命令与参数
		str_trim_crlf(sess->cmdline);
		//打印输出：
		printf("cmdline=[%s]\n", sess->cmdline);
		str_split(sess->cmdline,sess->cmd, sess->arg, ' ');
		str_upper(sess->cmd);
		
		//处理ftp命令
		const ftpcmd_t* ftp_ptr = ctrl_cmds;
		int found = 0; //cound find the command?
		while (ftp_ptr->cmd != NULL)
		{
			if (strcmp(ftp_ptr->cmd, sess->cmd) == 0)
			{
				found = 1;
				if (ftp_ptr->cmd_handler != NULL)
					ftp_ptr->cmd_handler(sess);//处理命令的回调函数执行
				else
					ftp_reply(sess, FTP_COMMANDNOTIMPL, "Unimplement command.");
				break;
			}
			
			ftp_ptr++;
		}
		
		if (found == 1)
			continue;
		else
			ftp_reply(sess, FTP_BADCMD, "Unknown command.");
	}
}



//ftp应答：传递状态和文本，返回给客户端
void ftp_reply(session_t* sess, int status, const char* text)
{
	char buf[1024] = {0};
	sprintf(buf, "%d %s\r\n", status, text);
	writen(sess->ctrl_fd, buf, strlen(buf));
}


//ftp应答：有'-'
void ftp_lreply(session_t* sess, int status, const char* text)
{
	char buf[1024]={0};
	sprintf(buf, "%d-%s\r\n", status, text);
	writen(sess->ctrl_fd, buf, strlen(buf));
}


//判断port是否建立
int port_active(session_t* sess)
{
	if (sess->p_sock != NULL)
		return 1;

	return 0;
}


//判断pasv是否建立
int pasv_active(session_t* sess)
{
	
	priv_sock_send_cmd(sess->ftp_fd, PRIV_SOCK_PASV_ACTIVE);
	int active = priv_sock_get_int(sess->ftp_fd);

	return active;
}


//得到传输的文件描述符
int get_transfer_fd(session_t* sess)
{
	// 判断是否接收过PORT 或者PASV命令
	if (!port_active(sess) && !pasv_active(sess))
		return 0;

	// 如果是主动模式，获取数据连接fd
	int ret = 1;
	if (port_active(sess))
	{
		if (pasv_active(sess))
		{
			fprintf(stderr, "both port and pasv are active.");
			exit(EXIT_FAILURE);
		}
		
		if (get_port_fd(sess) == 0)
			ret = 0;
	}
	
	// 如果是被动模式, 获取数据连接fd
	if (pasv_active(sess))
	{
		if (port_active(sess))
		{
			fprintf(stderr, "both port and pasv are active.");
			exit(EXIT_FAILURE);
		}

		if (get_pasv_fd(sess) == 0)
			ret = 0;
		
	}

	if (sess->p_sock)
	{
		free(sess->p_sock);
		sess->p_sock = NULL;
	}

	if (ret == 1)
	{
		start_data_alarm(); //数据通道创建完毕，开启数据通道闹钟
	}

	return ret;
}


//得到port模式的fd
int get_port_fd(session_t* sess)
{
	priv_sock_send_cmd(sess->ftp_fd, PRIV_SOCK_GET_DATA_SOCK);
	unsigned short port = ntohs(sess->p_sock->sin_port);
	char *ip = inet_ntoa(sess->p_sock->sin_addr);
	priv_sock_send_int(sess->ftp_fd, (int)port);
	priv_sock_send_buf(sess->ftp_fd, ip, strlen(ip));
	
	char res = priv_sock_get_result(sess->ftp_fd);
	if (res == PRIV_SOCK_RESULT_BAD)
	{
		return 0;
	}
	else if (res == PRIV_SOCK_RESULT_OK)
	{
		sess->data_fd = priv_sock_recv_fd(sess->ftp_fd);
	}

	return 1;
}


//得到pasv模式的fd
//通过发送请求给nobody进程，nobody进程接受连接并把连接的套接字发送给
//ftp服务进程
int get_pasv_fd(session_t* sess)
{
	priv_sock_send_cmd(sess->ftp_fd, PRIV_SOCK_PASV_ACCEPT);
	char res = priv_sock_get_result(sess->ftp_fd);

	if (res == PRIV_SOCK_RESULT_BAD)
	{
		return 0;
	}
	else if (res == PRIV_SOCK_RESULT_OK)
	{
		//得到数据传输通道的fd
		sess->data_fd = priv_sock_recv_fd(sess->ftp_fd);
	}

	return 1;
}

//返回list给客户端
void list_common(session_t* sess, int detail)
{
	//打开当前目录
	DIR* dir = opendir(".");
	if (dir == NULL)
	{
		return;
	}

	struct dirent* ent;	//dir的结构体
	struct stat sbuf;	//stat结构体
	//读取目录项的每一行
	while ((ent = readdir(dir)) != NULL)
	{
		if (lstat(ent->d_name, &sbuf) < 0)//获取文件相关信息
			continue;

		if (strncmp(ent->d_name, ".", 1) == 0)//是不是以.开头，如果是就忽略
            continue; //忽略隐藏文件
		//权限位
		const char* perms = statbuf_get_perms(&sbuf);
		
		char buf[1024] = {0};

		if (detail)
		{
			//权限，文件大小，权限
			int off = 0;
			off += sprintf(buf, "%s ", perms);
			off += sprintf(buf + off, " %3d %-8d %-8d ", (int)sbuf.st_nlink, sbuf.st_uid, sbuf.st_gid);
			off += sprintf(buf + off, "%8lu ", (unsigned long)sbuf.st_size);

			const char *datebuf = statbuf_get_date(&sbuf);

			off += sprintf(buf + off, "%s ", datebuf);
			if (S_ISLNK(sbuf.st_mode))
			{
				char tmp[1024] = {0};
				readlink(ent->d_name, tmp, sizeof(tmp));
				off += sprintf(buf + off, "%s -> %s\r\n", ent->d_name, tmp);
			}
			else
			{
				off += sprintf(buf + off, "%s\r\n", ent->d_name);
			}
		}
		else
			sprintf(buf, "%s\r\n", ent->d_name);
		
		writen(sess->data_fd, buf, strlen(buf));
	}

	closedir(dir);
	
}

void limit_rate(session_t *sess, unsigned int transfer_bytes, int up_load)
{
	//传输速度=传输字节数/传输时间
	//IF 当前传输速度 > 最大传输速度 THEN
	//睡眠时间 = (当前传输速度 / 最大传输速度 – 1) * 当前传输时间;
	sess->data_process = 1;//当前正在传输数据
	long tsec = get_curtime_sec();
	long tusec = get_curtime_usec();
	double elapsed;
	elapsed = (double)(tsec - sess->transfer_start_sec);
	elapsed += (double)(tusec - sess->transfer_start_usec) / (double)1000000;
	if (elapsed <= (double)0.0)
	{
		elapsed = (double)0.01;
	}
	
	unsigned int transfer_speed = (unsigned int)((double)transfer_bytes / elapsed);
	unsigned int rate_ratio;

	if (up_load)
	{
		if (transfer_speed <=  sess->upload_speed_max || sess->upload_speed_max == 0 )
		{
			sess->transfer_start_sec = get_curtime_sec();
			sess->transfer_start_usec = get_curtime_usec();
			return; //不需要限速
		}

		rate_ratio = transfer_speed / sess->upload_speed_max;
	}

	else
	{
		if (transfer_speed <=  sess->download_speed_max || sess->download_speed_max == 0)
		{
			sess->transfer_start_sec = get_curtime_sec();
			sess->transfer_start_usec = get_curtime_usec();
			return; //不需要限速
		}

		rate_ratio = transfer_speed / sess->download_speed_max;
	}

	double time_sleep = ((double)rate_ratio - (double)1) * elapsed;

	nano_sleep(time_sleep);
	printf("AAAAAAAAA\n");

	//更新时间
	sess->transfer_start_sec = get_curtime_sec();
	sess->transfer_start_usec = get_curtime_usec();

	
}

//上传
void upload_common(session_t* sess, int is_append)
{
	//创建数据连接（每次数据连接都是临时的，数据传输完毕就关闭套接字）
	if (get_transfer_fd(sess) == 0)
	{
		//425 响应
		ftp_reply(sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
		return;
	}

	long long offset = sess->restart_pos;
	printf("restart pos: %lld\n", offset);
	sess->restart_pos = 0;


	//打开文件
	int fd = open(sess->arg, O_CREAT | O_WRONLY, 0666);
	if (fd == -1)
	{
		ftp_reply(sess, FTP_UPLOADFAIL, "Could not create file.");
		return;
	}


	//文件加写锁
	int ret = lock_write_file(fd);
	if (ret == -1)
	{
		ftp_reply(sess, FTP_UPLOADFAIL, "Lock local file fail.");
		return;
	}

	if (!is_append && offset == 0) //STOR
	{
		ftruncate(fd, 0); //清空文件内容
		lseek(fd, 0, SEEK_SET); //定位到文件开头
	}

	else if (!is_append && offset != 0) //REST STOR
	{
		lseek(fd, offset, SEEK_SET); //定位到文件偏移位置
	}

	else if (is_append) //APPE
	{
		lseek(fd, 0, SEEK_END);//定位到文件末尾
	}
	
	ftp_reply(sess, FTP_DATACONN, "Ok to send data.");

	
	//读取数据套接字，写入本地文件
	
	int flag = 0;
	char buf[1024] = {0};


	//记录当前传输时间，供限速用
	sess->transfer_start_sec = get_curtime_sec();
	sess->transfer_start_usec = get_curtime_usec();

	while (1)
	{
		ret = read(sess->data_fd, buf, sizeof(buf));
		if (ret == -1)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else
			{
				flag = 2;
				break;
			}
		}
		else if (ret == 0)
		{
			flag = 0;
			break;
		}

		limit_rate(sess, ret, 1);
		if (sess->abor_received)
		{
			flag = 2;
			break;
		}

		if (writen(fd, buf, ret) != ret)
		{
			flag = 1;
			break;
		}
	}

	//传输结束，关闭数据连接
	if (!sess->abor_received)
	{
		close(sess->data_fd);
		sess->data_fd = -1;
	}
	
	unlock_file(fd);
	close(fd);

	if (flag == 0 && !sess->abor_received)
	{
		//226 应答
		ftp_reply(sess, FTP_TRANSFEROK, "Transfer complete.");
	}

	if (flag == 1)
	{
		// 451响应
		ftp_reply(sess, FTP_BADSENDFILE, "Fail to write to local file.");
	}

	else if (flag == 2)
	{
		// 426 响应
		ftp_reply(sess, FTP_BADSENDNET, "Fail to read from network stream.");
	}

	check_abor(sess);
	sess->data_process = 0;
	start_cmd_alarm(); //数据传输完毕，重启启动控制通道闹钟
	
}

//STOR
static void do_stor(session_t *sess)
{
	upload_common(sess, 0);
}

//RETR
//下载
static void do_retr(session_t *sess)
{
	//创建数据连接（每次数据连接都是临时的，数据传输完毕就关闭套接字）
	if (get_transfer_fd(sess) == 0)
	{
		//425 响应
		ftp_reply(sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
		return;
	}

	long long offset = sess->restart_pos;
	sess->restart_pos = 0;


	//打开文件
	int fd = open(sess->arg, O_RDONLY);
	if (fd == -1)
	{
		ftp_reply(sess, FTP_FILEFAIL, "Open local file fail.");
		return;
	}


	//文件加读锁
	int ret = lock_read_file(fd);
	if (ret == -1)
	{
		ftp_reply(sess, FTP_FILEFAIL, "Lock local file fail.");
		return;
	}

	//判断文件传输模式（实际上应该还要配置 ascii_upload_enable 和 ascii_download_enable 两个变量）
	//如果是ascii模式，则需要对\n转换成\r\n进行传输，在这边统一使用二进制方式传输，只是应答不同。
	char text[1024] = {0};
	struct stat sbuf;
	fstat(fd, &sbuf);
	
	//判断是否是普通文件（设备文件不能下载）
	if (!S_ISREG(sbuf.st_mode))
	{
		ftp_reply(sess, FTP_FILEFAIL, "It is not a regular file.");
		return;
	}

	if (!sess->is_ascii)
	{
		//150 响应
		sprintf(text, "Opening BINARY mode data connection for %s (%lld bytes).", 
			sess->arg, (long long)sbuf.st_size);	
	}
	else
	{
		//150 响应
		sprintf(text, "Opening ASCII mode data connection for %s (%lld bytes).", 
			sess->arg, (long long)sbuf.st_size);
	}

	ftp_reply(sess, FTP_DATACONN, text);

	//读取文件内容，写入数据套接字
	//#include <sys/sendfile.h>
    //sendfile()不涉及内核空间和用户空间的数据拷贝，效率比较高

	// 如果是断点续载，定位到文件的断点处
	if (offset != 0)
	{
		ret = lseek(fd, offset, SEEK_SET);
		if (ret == -1)
		{
			ftp_reply(sess, FTP_FILEFAIL, "Lseek local file fail.");
			return;
		}
	}
	
	int flag = 0;
	long long bytes_tosend = sbuf.st_size;
	if (offset != 0)
	{
		bytes_tosend -= offset;
	}

	//记录当前传输时间，供限速用
	sess->transfer_start_sec = get_curtime_sec();
	sess->transfer_start_usec = get_curtime_usec();

	while (bytes_tosend > 0)
	{
		int thistime_tosend = bytes_tosend > 4096 ? 4096: bytes_tosend;
		ret = sendfile(sess->data_fd, fd, NULL, thistime_tosend);
		if (ret == -1)
		{
			flag = 2;
			break;
		}
		
		limit_rate(sess, ret, 0); //限速
		if (sess->abor_received)
		{
			flag = 2;
			break;
		}

		bytes_tosend -= ret;
	}

	//传输结束，关闭数据连接
	if (!sess->abor_received)
	{
		close(sess->data_fd);
		sess->data_fd = -1;
	}
	
	unlock_file(fd);
	close(fd);

	if (flag == 0 && !sess->abor_received)
	{
		//226 应答
		ftp_reply(sess, FTP_TRANSFEROK, "Transfer complete.");
		
	}

	else if (flag == 2)
	{
		// 426 响应
		ftp_reply(sess, FTP_BADSENDNET, "Fail to write to network stream.");
	
	}
	
	check_abor(sess);	
	sess->data_process = 0;
	//start_cmd_alarm(); //数据传输完毕，重启启动控制通道闹钟	
}

//USER XXXX
static void do_user(session_t *sess)
{
	//从密码文件获取指定数据
	struct passwd* pw = getpwnam(sess->arg);
	if (pw == NULL)
	{
		ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
		return;
	}

	sess->uid = pw->pw_uid;
	ftp_reply(sess, FTP_GIVEPWORD, "Please specify passwd.");
	
}

//PASS ******
static void do_pass(session_t *sess)
{

	struct passwd* pw = getpwuid(sess->uid);//根据uid获取密码
	if (pw == NULL)
	{
		ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
		return;
	}

	struct spwd* sp = getspnam(pw->pw_name);//根据影子文件获取密码
	if (sp == NULL)
	{
		ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
		return;
	}

	//将客户端传递的用户密码加密后再比较
	//-lcrytp
	char* encrypt = crypt(sess->arg, sp->sp_pwdp);
	if (strcmp(encrypt, sp->sp_pwdp) != 0)
	{
		ftp_reply(sess, FTP_LOGINERR, "Login incorrect.");
		return;
	}
	
		umask(tunable_local_umask);

	//将当前进程的有效用户更改为XXXX
	if (setegid(pw->pw_gid) < 0)
		ERR_EXIT("setegid");
	if (seteuid(pw->pw_uid) < 0)
		ERR_EXIT("seteuid");
	chdir(pw->pw_dir); //更改当前目录为用户家目录

	ftp_reply(sess, FTP_LOGINOK, "Login successful.");
}

//CWD 
static void do_cwd(session_t *sess)
{
	//更改当前目录，调用chdir即可
	if (chdir(sess->arg) < 0)
	{
		ftp_reply(sess, FTP_NOPERM, "Failed to change directory.");

		return;
	}

	ftp_reply(sess, FTP_CWDOK, "Directory successfully changed.");	
}

//CDUP
static void do_cdup(session_t *sess)
{
	if (chdir("..") < 0)
	{
		ftp_reply(sess, FTP_NOPERM, "Failed to change directory.");

		return;
	}

	ftp_reply(sess, FTP_CWDOK, "Directory successfully changed.");
}

//QUIT
static void do_quit(session_t *sess)
{
	ftp_reply(sess, FTP_GOODBYE, "Goodbye.");
	exit(EXIT_SUCCESS);
}

//PORT
static void do_port(session_t *sess)
{
	// PORT 192,168,1,1,82,37
	//PORT模式需要将ip和端口记录下来以便后面连接到客户端
	unsigned int v[6];
	sscanf(sess->arg, "%u,%u,%u,%u,%u,%u", &v[2],&v[3],&v[4],&v[5],&v[0],&v[1]);

	sess->p_sock = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	memset(sess->p_sock, 0, sizeof(struct sockaddr_in));

	sess->p_sock->sin_family = AF_INET;
	unsigned char* p = (unsigned char*)&(sess->p_sock->sin_port);
	//网络字节序
	p[0] = v[0];
	p[1] = v[1];

	p = (unsigned char*)&(sess->p_sock->sin_addr);
	p[0] = v[2];
	p[1] = v[3];
	p[2] = v[4];
	p[3] = v[5];

	ftp_reply(sess, FTP_PORTOK, "PORT command successful. Consider using PASV.");
}

//PASV 
//pasv模式，被动模式，服务器发送ip和端口给客户端，让客户端连接
static void do_pasv(session_t *sess)
{
/*
PASV
227 Entering Passive Mode (192,168,1,22,166,254).
*/
	char ip[20] = {0};
	getlocalip(ip);

	priv_sock_send_cmd(sess->ftp_fd, PRIV_SOCK_PASV_LISTEN);
	unsigned short port = (unsigned short)priv_sock_get_int(sess->ftp_fd);


	unsigned int v[4];
	sscanf(ip, "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]);
	char buf[128] = {0};
	sprintf(buf, "Entering Passive Mode (%u,%u,%u,%u,%u,%u).", 
		v[0], v[1], v[2], v[3], port>>8, port&0xFF);

	ftp_reply(sess, FTP_PASVOK, buf);
}

//TYPE A/TYPE I
static void do_type(session_t *sess)
{
	
	if (strcmp(sess->arg, "A") == 0)
	{
		sess->is_ascii = 1;
		ftp_reply(sess, FTP_TYPEOK, "Switching to ASCII mode.");
	}

	else if (strcmp(sess->arg, "I") == 0)
	{
		sess->is_ascii = 0;
		ftp_reply(sess, FTP_TYPEOK, "Switching to BINARY mode.");
	}

	else
	{
		ftp_reply(sess, FTP_BADCMD, "Unrecognised TYPE command.");
	}
}


//APPE
static void do_appe(session_t *sess)
{
	upload_common(sess, 1);
}

//LIST 
static void do_list(session_t *sess)
{
	//创建数据连接
	if (get_transfer_fd(sess) == 0)
	{
		//425 响应
		ftp_reply(sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
		return;
	}

	//150 响应
	ftp_reply(sess, FTP_DATACONN, "Here comes the directory listing.");

	//开始传输列表
	list_common(sess, 1);

	//传输结束，关闭数据连接
	close(sess->data_fd);
	sess->data_fd = -1;

	//226 应答
	ftp_reply(sess, FTP_TRANSFEROK, "Directory send OK.");
}

//只传输基本信息
static void do_nlst(session_t *sess)
{
	//创建数据连接
	if (get_transfer_fd(sess) == 0)
	{
		//425 响应
		ftp_reply(sess, FTP_BADSENDCONN, "Use PORT or PASV first.");
		return;
	}

	//150 响应
	ftp_reply(sess, FTP_DATACONN, "Here comes the directory listing.");

	//开始传输列表
	list_common(sess, 0);

	//传输结束，关闭数据连接
	close(sess->data_fd);
	sess->data_fd = -1;

	//226 应答
	ftp_reply(sess, FTP_TRANSFEROK, "Directory send OK.");
}

//记录续传的位置
static void do_rest(session_t *sess)
{
	sess->restart_pos = str_to_longlong(sess->arg);
	char buf[1024] = {0};
	sprintf(buf, "Restart position accepted (%lld).", sess->restart_pos);
	
	ftp_reply(sess, FTP_RESTOK, buf);
}

//ABOR
static void do_abor(session_t *sess)
{
	ftp_reply(sess, FTP_ABOR_NOCONN, "No transfer to abor");
}

//PWD
static void do_pwd(session_t *sess)
{
	char buf[1024] = {0};
	char home[1024] = {0};
	//获取家目录
	getcwd(home, sizeof(home));
	sprintf(buf, "\"%s\"", home);

	ftp_reply(sess, FTP_PWDOK, buf);
}

//创建目录
static void do_mkd(session_t *sess)
{
	if (mkdir(sess->arg, 0777) < 0)
	{
		ftp_reply(sess, FTP_FILEFAIL, "Create directory operation fail.");
		return;
	}

	char text[4096] = {0};
	//绝对路径
	if (sess->arg[0] == '/')
	{
		sprintf(text, "\"%s\" created", sess->arg);
	}
	//相对路径
	else
	{
		char dir[4096] = {0};
		getcwd(dir, 4096);
		//判断是否以'/'结尾
		if (dir[strlen(dir)-1] == '/')
		{
			sprintf(text, "\"%s%s\" created", dir, sess->arg);
		}
		else
			sprintf(text, "\"%s/%s\" created", dir, sess->arg);
	}

	ftp_reply(sess, FTP_MKDIROK, text);	
}

//删除目录
static void do_rmd(session_t *sess)
{
	if (rmdir(sess->arg) < 0)
	{
		ftp_reply(sess, FTP_NOPERM, "Create directory operation fail.");
		return;
	}
	
	ftp_reply(sess, FTP_RMDIROK, "Create directory operation successful.");
}

//DELE
static void do_dele(session_t *sess)
{
	//删除文件使用unlink
	if (unlink(sess->arg) < 0)
	{
		ftp_reply(sess, FTP_NOPERM, "Delete operation fail.");
		return;
	}
	
	ftp_reply(sess, FTP_DELEOK, "Delete operation successful.");
}

//RNFR 重命名1
static void do_rnfr(session_t *sess)
{
	sess->rnfr_name = (char*)malloc(strlen(sess->arg)+1);
	memset(sess->rnfr_name, 0, strlen(sess->arg)+1);
	strcpy(sess->rnfr_name, sess->arg);
	
	ftp_reply(sess, FTP_RNFROK, "Ready for RNTO.");	
}

//RNTO 重命名2
static void do_rnto(session_t *sess)
{
	if (sess->rnfr_name == NULL)
	{
		ftp_reply(sess, FTP_NEEDRNFR, "RNFR require first.");
		return;
	}

	rename(sess->rnfr_name, sess->arg);//重命名
	ftp_reply(sess, FTP_RENAMEOK, "Rename successful.");
	free(sess->rnfr_name);
	sess->rnfr_name = NULL; //置为NULL	
}

//修改权限
static void do_site_chmod(session_t* sess, char* arg)
{
	if (strlen(arg) == 0)
	{
			ftp_reply(sess, FTP_BADCMD, "SITE CHMOD need 2 arguments.");
			return;
	}

	char perm[100] = {0};
	char file[100] = {0};
	str_split(arg, perm, file, ' ');
	if (strlen(file) == 0)
	{
		ftp_reply(sess, FTP_BADCMD, "SITE CHMOD need 2 arguments.");
		return;
	}
	
	unsigned int mode = str_octal_to_uint(perm);
	if (chmod(file, mode) < 0)
	{
		ftp_reply(sess, FTP_BADCMD, "SITE CHMOD Command fail.");
		return;
	}

	ftp_reply(sess, FTP_CHMODOK, "SITE CHMOD Command Ok.");

}

//修改umask
static void do_site_umask(session_t* sess, char* arg)
{
	if (strlen(arg) == 0)
	{
		char text[1024] = {0};
		sprintf(text, "Your current UMASK is 0%o", tunable_local_umask);
		ftp_reply(sess, FTP_UMASKOK, text);
		return;
	}

	unsigned int mode = str_octal_to_uint(arg);
	umask(mode);//要先将权限位转为10机制
	char text[1024] = {0};
	sprintf(text, "UMASK set to 0%o", mode);
	ftp_reply(sess, FTP_UMASKOK, text);

}

//SITE
static void do_site(session_t *sess)
{
	char cmd[100] = {0};
	char arg[100] = {0};
	str_split(sess->arg, cmd, arg, ' ');

	if (strcmp(cmd, "CHMOD") == 0)
	{
		 // SITE CHMOD <perm> <file>
		do_site_chmod(sess, arg);
	}

	else if (strcmp(cmd, "UMASK") == 0)
	{
		 // SITE UMASK [umask]
		do_site_umask(sess, arg);
	}

	else if (strcmp(cmd, "HELP") == 0)
	{
		 // SITE HELP
		ftp_reply(sess, FTP_SITEHELP, "CHMOD UMASK HELP");
	}

	else
		ftp_reply(sess, FTP_BADCMD, "Unknown SITE command.");
}

//SYST
static void do_syst(session_t *sess)
{
	ftp_reply(sess, FTP_SYSTOK, "UNIX Type: L8");
}

//FEAT 
//发送一些服务端的特性给客户端，直接写入就可以了
static void do_feat(session_t *sess)
{
	ftp_lreply(sess, FTP_HELP, "The following commands are recognized.");
	writen(sess->ctrl_fd, " ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n",
		strlen(" ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n"));

	writen(sess->ctrl_fd, " MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n",
		strlen(" MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n"));

	writen(sess->ctrl_fd, " RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n",
		strlen(" RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n"));

	writen(sess->ctrl_fd, " XPWD XRMD\r\n",
		strlen(" XPWD XRMDr\n"));

	ftp_reply(sess, FTP_HELP, "Help OK.");
}

//SIZE XXX
//获取文件大小
static void do_size(session_t *sess)
{
	struct stat sbuf;
	if (stat(sess->arg, &sbuf) < 0)
	{
		ftp_reply(sess, FTP_FILEFAIL, "SIZE operation fail.");
		return;
	}

	//是不是普通文件
	if (!S_ISREG(sbuf.st_mode))
	{
		ftp_reply(sess, FTP_FILEFAIL, "Could not get file size.");
		return;
	}
	
	char buf[20] = {0};
	sprintf(buf, "File Size: %lld", (long long)sbuf.st_size);
	ftp_reply(sess, FTP_SIZEOK, buf);
}

//STAT 
//获取客户端的状态
static void do_stat(session_t *sess)
{
		ftp_lreply(sess, FTP_STATOK, "FTP server status:");

	if (sess->upload_speed_max == 0)
	{
		writen(sess->ctrl_fd, "\t No session upload bandwidth limit\r\n",
		strlen("\t No session upload bandwidth limit\r\n"));
	}
	else if (sess->upload_speed_max > 0)
	{
		char text[1024] = {0};
		sprintf(text, "\t session upload bandwidth limit is %u b/s\r\n", sess->upload_speed_max);
		writen(sess->ctrl_fd, text, strlen(text));
	}

	if (sess->download_speed_max == 0)
	{
		writen(sess->ctrl_fd, "\t No session download bandwidth limit\r\n",
		strlen("\t No session download bandwidth limit\r\n"));
	}
	else if (sess->download_speed_max > 0)
	{
		char text[1024] = {0};
		sprintf(text, "\t session download bandwidth limit is %u b/s\r\n", sess->download_speed_max);
		writen(sess->ctrl_fd, text, strlen(text));
	}

	writen(sess->ctrl_fd, "\t Control connection is plain text\r\n",
		strlen("\t Control connection is plain text\r\n"));

	writen(sess->ctrl_fd, "\t Data connections will be plain text\r\n",
		strlen("\t Data connections will be plain text\r\n"));

	char text[1024] = {0};
		sprintf(text, "\t At session startup, client count was %u\r\n", sess->num_clients);
		writen(sess->ctrl_fd, text, strlen(text));

	writen(sess->ctrl_fd, "\t miniFTPd 0.1 - secure, fast, stable\r\n",
		strlen("\t miniFTPd 0.1 - secure, fast, stable\r\n"));

	ftp_reply(sess, FTP_STATOK, "End of status");
}

//NOOP
static void do_noop(session_t *sess)
{
	ftp_reply(sess, FTP_NOOPOK, "NOOP Ok.");
}

//HELP
static void do_help(session_t *sess)
{
	ftp_lreply(sess, FTP_HELP, "The following commands are recognized.");
	writen(sess->ctrl_fd, " ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n",
		strlen(" ABOR ACCT ALLO APPE CDUP CWD  DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n"));

	writen(sess->ctrl_fd, " MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n",
		strlen(" MODE NLST NOOP OPTS PASS PASV PORT PWD  QUIT REIN REST RETR RMD  RNFR\r\n"));

	writen(sess->ctrl_fd, " RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n",
		strlen(" RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n"));

	writen(sess->ctrl_fd, " XPWD XRMD\r\n",
		strlen(" XPWD XRMDr\n"));

	ftp_reply(sess, FTP_HELP, "Help OK.");
}