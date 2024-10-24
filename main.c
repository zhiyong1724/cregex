#include <stdio.h>
#include <time.h>
#include "cregex.h"
int main()
{   
    CRegex *regex = cRegexCompile("^[ABC]{0}[A-Z]{1}[^Z]{1,}\\[{0,2}(abc|acd)?\\s.*\\bhello\\b\\s+\\W*\\S*\\D*\\w+\\s+\\d+(a{1,}){2,6}$");
    if (NULL == regex)
        return -1;
    CRegexMatch matchs[2];
    clock_t begin = clock();
    int result = cRegexSearch(regex, "AB[acd 正 hello  则e  123aaaa", matchs, 2, 0);
    clock_t end = clock();
    printf("result = %d\n", result);
    printf("us:%ld\n", end - begin);
    cRegexFree(regex);
    return 0;
}