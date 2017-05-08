#include "parseconf.h"
#include "tunable.h"
#include "common.h"
#include "str.h"

//配置文件的数组，每一个选项都是一个字符串加一个变量
//当解析到的字符串与第一个字符相同时
//把第二个变量设置为配置设置的值。
static BOOL_CONFIG parseconf_bool_array[] =
{
	{ "pasv_enable", &tunable_pasv_enable },
	{ "port_enable", &tunable_port_enable },
	{ NULL, NULL }
};

static UINT_CONFIG parseconf_uint_array[] =
{
	{ "listen_port", &tunable_listen_port },
	{ "max_clients", &tunable_max_clients },
	{ "max_per_ip", &tunable_max_per_ip },
	{ "accept_timeout", &tunable_accept_timeout },
	{ "connect_timeout", &tunable_connect_timeout },
	{ "idle_session_timeout", &tunable_idle_session_timeout },
	{ "data_connection_timeout", &tunable_data_connection_timeout },
	{ "local_umask", &tunable_local_umask },
	{ "upload_max_rate", &tunable_upload_max_rate },
	{ "download_max_rate", &tunable_download_max_rate },
	{ NULL, NULL }
};

static STR_CONFIG parseconf_str_array[] =
{
	{ "listen_address", &tunable_listen_address },
	{ NULL, NULL }
};


void parseconf_load_file(const char *path)
{
	FILE* fp = fopen(path, "r");
	if (fp == NULL)
		ERR_EXIT("fopen");

	char setting_line[1024] = {0};

	while (fgets(setting_line, sizeof(setting_line), fp) != NULL)//读取一行
	{
		if (strlen(setting_line) == 0 ||
			setting_line[0] == '#' ||
			str_all_space(setting_line))
			continue;
		
		str_trim_crlf(setting_line); //去除\r\n
		parseconf_load_setting(setting_line); //解析配置文件
		memset(setting_line, 0, sizeof(setting_line));
	}

	fclose(fp);
}

void parseconf_load_setting(const char *setting)
{
	char key[128] = {0};
	char value[128]= {0};
	//去除空格
	while (isspace(*setting))
		setting++;

	str_split(setting, key, value, '='); //分割一行配置

	if (strlen(value) == 0) //没有'='或者value
	{
		fprintf(stderr, "missing value of %s\n", key);
		exit(EXIT_FAILURE);
	}

// 遍历三个数组，如果能够匹配key，将value赋值给对应的变量并返回
	{
		//bool类型的配置解析
		const BOOL_CONFIG* ptr_bool = parseconf_bool_array;
		while (ptr_bool->p_setting_name != NULL)
		{
			if (strcmp(key, ptr_bool->p_setting_name) == 0)
			{	
				str_upper(value);

				if (strcmp(value, "YES")
					|| strcmp(value, "TRUE")
					|| strcmp(value, "1"))

					*(ptr_bool->p_variable) = 1;

				else if (strcmp(value, "NO")
					|| strcmp(value, "FALSE")
					|| strcmp(value, "0"))
				
					*(ptr_bool->p_variable) = 0;
				else
				{
					fprintf(stderr, "bad bool value of %s\n", key);
					exit(EXIT_FAILURE);
				}

				return;
			}
			
			ptr_bool++;
		}
	}


	{
		const UINT_CONFIG* ptr_uint = parseconf_uint_array;
		while (ptr_uint->p_setting_name != NULL)
		{
			if (strcmp(key, ptr_uint->p_setting_name) == 0)
			{
				//八进制的配置第一个数可能为0，所以这里要判断是八进制还是十进制
				if (value[0] == '0')
					*(ptr_uint->p_variable) = str_octal_to_uint(value);
				else
					*(ptr_uint->p_variable) = (unsigned int)(atoi(value));

				return ;
			}
			
			ptr_uint++;
		}
	}


	{
		const STR_CONFIG* ptr_str = parseconf_str_array;
		while (ptr_str->p_setting_name != NULL)
		{
			if (strcmp(key, ptr_str->p_setting_name) == 0)
			{
				const char** p_cur = ptr_str->p_variable;
				if (*p_cur != NULL)
					free((char*)(*p_cur));
				*p_cur = strdup(value);//申请空间并赋值值p_cur

				return;
			}
			
			ptr_str++;
		}
	}

}

