#include "privparent.h"
#include "common.h"
#include "sysutil.h"
#include "tunable.h"
#include "privsock.h"


//根据接收的ip和port绑定端口20并连接到客户端，然后将连接的fd发送给ftpproto
static void privop_port_get_data_sock(session_t *sess);
//获取PASV的状态并发送给ftpproto进程
static void privop_pasv_active(session_t *sess);
//应答监听端口:启动本地监听，并把监听的端口发送个fptproto
static void privop_pasv_listen(session_t *sess);
//pasv模式下接收一个连接，并把接收的连接发送给ftpproto
static void privop_pasv_accept(session_t *sess);


//声明
int capset(cap_user_header_t hdrp, const cap_user_data_t datap);
// capset是一个原始的系统接口函数，但是在头文件<linux/capability>是没有定义的
// 在这里我们重新使用系统调用来实现
int capset(cap_user_header_t hdrp, const cap_user_data_t datap)
{
	return syscall(__NR_capset, hdrp, datap);//__NR_capset 是系统调用号
}
// 使nobody进程拥有bind socket的权限
void minimize_privilege(void)
{
	//将当前进程的有效用户更改为nobody
	struct passwd* pw = getpwnam("nobody");
	if (pw == NULL)
		ERR_EXIT("getpwnam");

	if (setegid(pw->pw_gid) < 0)
		ERR_EXIT("setegid");
	if (seteuid(pw->pw_uid) < 0)
		ERR_EXIT("seteuid");

	// man 7 capabilities
	// #include <linux/capability.h>
	struct __user_cap_header_struct cap_header;
	struct __user_cap_data_struct cap_data;
	memset(&cap_header, 0, sizeof(cap_header));
	memset(&cap_data, 0, sizeof(cap_data));

	cap_header.version = _LINUX_CAPABILITY_VERSION_1;
	cap_header.pid = 0;
	
	__u32 cap_mask = 0;
	cap_mask |= (1 << CAP_NET_BIND_SERVICE);
    cap_data.effective = cap_mask;//类似于权限的集合  
    cap_data.permitted = cap_mask;//0001000000  
    cap_data.inheritable = 0;//子进程不继承特权  

	capset(&cap_header, &cap_data);
}

//子进程发送过来的消息(该消息使用内部命令)
void handle_parent(session_t *sess)
{
	minimize_privilege();
	
	char cmd;
	while(1)
	{
		//获取fpt协议进程传递的消息
		cmd = priv_sock_get_cmd(sess->nobody_fd);
		switch (cmd)
		{
		case PRIV_SOCK_GET_DATA_SOCK:
			privop_port_get_data_sock(sess);
			break;

		case PRIV_SOCK_PASV_ACTIVE:
			privop_pasv_active(sess);
			break;

		case PRIV_SOCK_PASV_LISTEN:
			privop_pasv_listen(sess);
			break;

		case PRIV_SOCK_PASV_ACCEPT:
			privop_pasv_accept(sess);
			break;
		}

	}
}

//根据发送过来的客户端ip和端口号，绑定20端口并与客户端连接，
//此时的C端为ftp服务进程，S端为客户端。
static void privop_port_get_data_sock(session_t *sess)
{
	//先获取ip和端口号
	unsigned short port = (unsigned short)priv_sock_get_int(sess->nobody_fd);
	char ip[20] = {0};
	priv_sock_recv_buf(sess->nobody_fd, ip, sizeof(ip));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

// 当多个客户端连接服务器时，可以同时有多个nobody进程bind 20端口+ip，因为此时客户端给出的端口是随机的，故
// 四元组还是能识别一个数据连接
// 当客户端位于NAT服务器之后使用PORT模式时，需要在NAT服务器上配置映射才能让服务器主动连接到客户端
// 所以采用的是标准固定的20端口，这样NAT服务器就不用维护很多的表目
	int fd = tcp_client(20);
	if (fd == -1)
	{
		priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_BAD);
		return;
	}
//连接
	if (connect_timeout(fd, &addr, tunable_connect_timeout) < 0)
	{
		close(fd);
		priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_BAD);
		return;
	}
//发送结果和fd给ftp协议解析进程
	priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_OK);
	priv_sock_send_fd(sess->nobody_fd, fd);
	close(fd); //父进程必须close(fd)，否则子进程关闭close(data_fd)的时候也不会发送FIN段。
	//因为通过unix域协议传递套接字等于是对方也打开套接字，而直接简单的赋值是没有这样的效果的。
}

//判断是不是处于pasv模式
static void privop_pasv_active(session_t *sess)
{
	//根据监听套接字是否为1，判断是不是处于pasv模式
	int active;
	if (sess->pasv_listen_fd != -1)//监听套接字
		active = 1;
	else
		active = 0;
	priv_sock_send_int(sess->nobody_fd, active);
}

//启动一个服务端进行监听，并把监听的端口发送个ftp协议解析进程
//因为该进程能够获得本地ip地址，所以只需发送端口号即可
static void privop_pasv_listen(session_t *sess)
{
	char ip[20] = {0};
	getlocalip(ip);
	// 服务器 bind 的必须是随机端口，因此此时可能有多个nobody 进程在listen, 这样才不会冲突
	sess->pasv_listen_fd = tcp_server(ip, 0);
	struct sockaddr_in localaddr;
	socklen_t addrlen = sizeof(localaddr);
	if (getsockname(sess->pasv_listen_fd, (struct sockaddr *)&localaddr, &addrlen) < 0)
		ERR_EXIT("getsockname");

	unsigned short port = ntohs(localaddr.sin_port);

	priv_sock_send_int(sess->nobody_fd, (int)port);
}

//接受连接，并把连接的fd发送给ftp服务进程
//返回连接的结果和fd给ftp服务进程
static void privop_pasv_accept(session_t *sess)
{
	// 因为服务器端给出的端口是随机的，accept 返回数据套接字关联的端口是进程独立的
	//在上一步listen时，已经保存了listen_fd，所以直接连接即可
	int fd = accept_timeout(sess->pasv_listen_fd, NULL, tunable_accept_timeout);
	close(sess->pasv_listen_fd);
	sess->pasv_listen_fd = -1;
	if (fd < 0)
	{
		priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_BAD);
		return;
	}

	priv_sock_send_result(sess->nobody_fd, PRIV_SOCK_RESULT_OK);
	priv_sock_send_fd(sess->nobody_fd, fd);
	close(fd);
}