#include "str.h"

//移除\r\n
void str_trim_crlf(char *str)
{
	char* p = &str[strlen(str)-1];
	while (*p == '\r' || *p == '\n')
		*p-- = '\0';
	
}

//使用字符c分割str，其中左边放入left，右边放入right
void str_split(const char *str , char *left, char *right, char c)
{
	char* p = strchr(str, c);//找到字符c的位置
	if (p == NULL)//没有找到该字符，直接复制给left
		strcpy(left, str);
	else
	{
		strncpy(left, str, p-str);
		strcpy(right, p+1);
	}	
}

//判断字符串str是不是全是空
int str_all_space(const char *str)
{
	while(*str)
	{
		if (!isspace(*str))
			return 0;

		str++;
	}

	return 1;	
}


//将字符串转为大写
void str_upper(char *str)
{
	while (*str)
	{
		*str = toupper(*str);
		str++;
	}	
}

//将字符串转为long long int
long long str_to_longlong(const char *str)
{
	int i;
	long long result = 0;
	long long mul = 1;
	int len = strlen(str);

	for (i = len-1; i >= 0; i-- )
	{
		char ch = str[i];
		if (ch < '0' || ch > '9')
			return 0;

		result += (ch - '0') * mul;
		mul *= 10;
	}

	return result;
}

//将八进制的str转为int
unsigned int str_octal_to_uint(const char *str)
{
	unsigned int result = 0;
	//去除前导的0
	while (isdigit(*str) && *str == '0')
		str++;

	while (*str)
	{
		char ch = *str;
		if (ch < '0' || ch > '7')
			return 0;

		result <<= 3;
		result += ch - '0';
		str++;
	}

	return result;	
}