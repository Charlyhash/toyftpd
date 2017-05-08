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

//得到传输的文件描述符
int get_transfer_fd(session_t* sess)
{
	
	return 0;
}
//得到port模式的fd
int get_port_fd(session_t* sess)
{
	return 0;
}

//得到pasv模式的fd
int get_pasv_fd(session_t* sess)
{
	
	return 0;
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
//下载
void upload_common(session_t* sess, int is_append)
{
	
	
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

static void do_cwd(session_t *sess)
{}
static void do_cdup(session_t *sess)
{}
static void do_quit(session_t *sess)
{}
static void do_port(session_t *sess)
{}

//PASV 
//pasv模式，被动模式，服务器发送ip和端口给客户端，让客户端连接
static void do_pasv(session_t *sess)
{
	//响应 227 Entering Passive Mode (192,168,56,188,49,137).
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

//TYPE A
static void do_type(session_t *sess)
{
	//此处只实现了ascii模式
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

static void do_retr(session_t *sess)
{}
static void do_stor(session_t *sess){}
static void do_appe(session_t *sess){}
static void do_list(session_t *sess){}
static void do_nlst(session_t *sess){}
static void do_rest(session_t *sess){}
static void do_abor(session_t *sess){}

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
static void do_mkd(session_t *sess){}
static void do_rmd(session_t *sess){}
static void do_dele(session_t *sess){}
static void do_rnfr(session_t *sess){}
static void do_rnto(session_t *sess){}
static void do_site(session_t *sess){}

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
static void do_size(session_t *sess){}
static void do_stat(session_t *sess){}
static void do_noop(session_t *sess){}
static void do_help(session_t *sess){}