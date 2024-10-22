# cregex
# 一个用标准C写的小型正则表达式引擎
### 代码仓库
https://github.com/zhiyong1724/cregex.git
### 简介
cregex是一个用标准C写的小型正则表达式引擎，使用NFA（非确定性有限状态机）构造对象树和动态DFA（确定性有限状态机）匹配算法。
动态内存占用跟正则表达式pattern长度呈正相关，在不考虑小括号()嵌套的状态下，最大的递归次数是个常数，如果不提取子字符串，不需要进行回溯，即便提取子字符串也仅仅需要进行一次回溯，最大的时间复杂度是O(mn)。
### 支持语法
|元字符|描述|
|:-:|:-|
|(abc)|分组，可用于子字符串的提取和提高优先级|
|[abc]|匹配'a''b''c'中的一个字符|
|[^abc]|匹配除'a''b''c'中的一个字符|
|[0-9]|匹配’0‘-‘9’中的一个字符|
|{m}|重复m次|
|{m，}|重复m次或更多次|
|{m,n}|重复m次到n次|
|*|重复0次或更多次|
|+|重复1次或更多次|
|?|重复0次或1次|
|abc\|acd|匹配abc或者acd|
|.|匹配除换行符外所有字符|
|^|匹配字符串开头|
|$|匹配字符串结尾|
|\w|匹配所有字母或者数字或者下划线_|
|\W|\w的反义|
|\s|匹配任意空格符号|
|\S|\s的反义|
|\d|匹配所有数字|
|\D|\d的反义|
|\b|匹配单词的开头或者结尾|
|\B|\b的反义|
### 子字符串提取
仅支持一级子字符串的提取，不支持嵌套()中的子字符串提取((ab)cd)e中只能提取abcd。
### 示例
```c
#include <stdio.h>
#include <time.h>
#include "cregex.h"
int main()
{   
    CRegex *regex = cRegexCompile("(a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?)aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    if (NULL == regex)
        return -1;
    CRegexMatch matchs[2];
    clock_t begin = clock();
    int result = cRegexMatch(regex, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", matchs, 2, 0);
    clock_t end = clock();
    printf("result = %d\n", result);
    printf("us:%ld\n", end - begin);
    cRegexFree(regex);
    return 0;
}
```