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