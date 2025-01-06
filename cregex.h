#ifndef __CREGEX_H__
#define __CREGEX_H__
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define CREGEX_FLAG_DEFAULT 0
#define CREGEX_FLAG_NOSUB 1
typedef struct cregex_t cregex_t;
typedef struct cregex_match_t
{
    size_t begin;
    size_t len;
} cregex_match_t;
/*********************************************************************************************************************
* regex 正则表达式对象
* pattern 正则表达式
* return 正则表达式对象
*********************************************************************************************************************/
cregex_t *cregex_compile(const char *pattern);
/*********************************************************************************************************************
* 释放正则表达式
* regex 正则表达式对象
*********************************************************************************************************************/
void cregex_free(cregex_t *regex);
/*********************************************************************************************************************
* 完全匹配
* regex 正则表达式对象
* text 匹配文本
* matchs 返回匹配子串位置
* n_match matchs数量
* flag 标志位，默认值为零
* return 0：匹配成功
*********************************************************************************************************************/
int cregex_match(cregex_t *regex, const char *text, cregex_match_t *matchs, size_t n_match, int flag);
/*********************************************************************************************************************
* 找到匹配的字符串，可以一直往后查找，直到返回-1
* regex 正则表达式对象
* text 匹配文本
* matchs 返回匹配子串位置
* n_match matchs数量
* flag 标志位，默认值为CREGEX_FLAG_DEFAULT
* return 0：查找成功
*********************************************************************************************************************/
int cregex_search(cregex_t *regex, const char *text, cregex_match_t *matchs, size_t n_match, int flag);
#ifdef __cplusplus
}
#endif
#endif