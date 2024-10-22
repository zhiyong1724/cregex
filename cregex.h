#ifndef __CREGEX_H__
#define __CREGEX_H__
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define CREGEX_FLAG_DEFAULT 0
#define CREGEX_FLAG_NOSUB 1
typedef struct CRegex CRegex;
typedef struct CRegexMatch
{
    size_t begin;
    size_t len;
} CRegexMatch;
/*********************************************************************************************************************
* regex 正则表达式对象
* pattern 正则表达式
* return 正则表达式对象
*********************************************************************************************************************/
CRegex *cRegexCompile(const char *pattern);
/*********************************************************************************************************************
* 释放正则表达式
* regex 正则表达式对象
*********************************************************************************************************************/
void cRegexFree(CRegex *regex);
/*********************************************************************************************************************
* 完全匹配
* regex 正则表达式对象
* text 匹配文本
* matchs 返回匹配子串位置
* nMatch matchs数量
* flag 标志位，默认值为零
* return 0：匹配成功
*********************************************************************************************************************/
int cRegexMatch(CRegex *regex, const char *text, CRegexMatch *matchs, size_t nMatch, int flag);
/*********************************************************************************************************************
* 找到匹配的字符串，可以一直往后查找，直到返回-1
* regex 正则表达式对象
* text 匹配文本
* matchs 返回匹配子串位置
* nMatch matchs数量
* flag 标志位，默认值为CREGEX_FLAG_DEFAULT
* return 0：查找成功
*********************************************************************************************************************/
int cRegexSearch(CRegex *regex, const char *text, CRegexMatch *matchs, size_t nMatch, int flag);
#ifdef __cplusplus
}
#endif
#endif