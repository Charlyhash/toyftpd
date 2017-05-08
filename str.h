#ifndef _STR_H_
#define _STR_H_
#include "common.h"

//移除\r\n
void str_trim_crlf(char *str);
//使用字符c分割str，其中左边放入left，右边放入right
void str_split(const char *str , char *left, char *right, char c);
//判断字符串str是不是全是空
int str_all_space(const char *str);
//将字符串转为大写
void str_upper(char *str);
//将字符串转为long long int
long long str_to_longlong(const char *str);
//将八进制的str转为int
unsigned int str_octal_to_uint(const char *str);


#endif