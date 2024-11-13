#include <stdio.h>
#include <time.h>
#include "cregex.h"
static void dump(const char *text, CRegexMatch *matchs, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        printf("%ld: ", i);
        for (size_t j = 0; j < matchs[i].len; j++)
        {
            printf("%c", text[matchs[i].begin + j]);
        }
        printf("\n");
    }
}

int main()
{   
    CRegex *regex = cRegexCompile("^[ABC]{0}[A-Z]{1}[^Z]{1,}\\[{0,2}(abc|acd)?\\s.*\\bhello\\b\\s+\\W*\\S*\\D*\\w+\\s+\\d+(a{1,}){2,6}$");
    const char *text = "AB[acd 正 hello  则e  123aaaa";
    if (NULL == regex)
        return -1;
    CRegexMatch matchs[8];
    clock_t begin = clock();
    int result = cRegexSearch(regex, text, matchs, 8, 0);
    clock_t end = clock();
    dump(text, matchs, 8);
    printf("result = %d\n", result);
    printf("us:%ld\n", end - begin);
    cRegexFree(regex);
    return 0;
}