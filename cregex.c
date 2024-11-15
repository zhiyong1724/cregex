#include "cregex.h"
#include <string.h>
#include <stdlib.h>
enum
{
    MATCH = 0,
    SPLIT = 259,
    WORD_MARGIN = 258,
    STR_START = 257,
    STR_END = 256,
};
#define CHAR_SET_SIZE SPLIT

typedef struct Repeat
{
    int min;
    int max;
} Repeat;

#define MAX_GROUPS 8
typedef struct State
{
    int c;
    struct State *out;
    struct State *out1;
    struct State *copy;
    int groups[MAX_GROUPS];
    int visited;
    int visited1;
    struct State *del;
} State;

typedef struct Fragment
{
    State *state;
    State **next;
} Fragment;

typedef struct FragmentStack
{
    Fragment stack[3];
    size_t n;
} FragmentStack;

typedef struct CharSet
{
    char c[CHAR_SET_SIZE];
} CharSet;

typedef struct StateSet
{
    State **states;
    size_t count;
    struct StateSet *next;
} StateSet;

struct CRegex
{
    State matchState;
    State *root;
    StateSet next;
    StateSet cmp;
    int groups[MAX_GROUPS];
    int index;
    int visited;
    int visited1;
};

static void initCharSet(CharSet *charSet, int inverse)
{
    memset(charSet, inverse, CHAR_SET_SIZE);
    if (inverse > 0)
    {
        charSet->c[WORD_MARGIN] = 0;
        charSet->c[STR_START] = 0;
        charSet->c[STR_END] = 0;
    }
}

static void initFragmentStack(FragmentStack *stack)
{
    stack->n = 0;
}

static void pushFragmentStack(FragmentStack *stack, Fragment *fragment)
{
    stack->stack[stack->n++] = *fragment;
}

static Fragment popFragmentStack(FragmentStack *stack)
{
    return stack->stack[--stack->n];
}

static State *newState(int c, State *out, State *out1, int *groups, int deep)
{
    State *state = (State *)malloc(sizeof(State));
    if (state != NULL)
    {
        state->c = c;
        state->out = out;
        state->out1 = out1;
        state->copy = NULL;
        for (size_t i = 0; i < MAX_GROUPS; i++)
        {
            state->groups[i] = -1;
        }
        for (; deep >= 0 && deep < MAX_GROUPS; deep--)
        {
            state->groups[deep] = groups[deep] - 1;
        }
        state->visited = 0;
        state->visited1 = 0;
        state->del = NULL;
        return state;
    }
    return NULL;
}

static void freeState(State *state, int visited, int visited1)
{
    State *del = NULL;
    State *last = NULL;
    State *cur = state;
    while (cur != NULL && cur->visited != visited)
    {
        cur->visited1 = visited1;
        if (cur->out != NULL && cur->out->visited != visited && cur->out->visited1 != visited1)
        {
            cur = cur->out;
        }
        else if (cur->out1 != NULL && cur->out1->visited != visited && cur->out1->visited1 != visited1)
        {
            cur = cur->out1;
        }
        else
        {
            if (del != NULL)
            {
                last->del = cur;
                last = cur;
            }
            else
            {
                del = last = cur;
            }
            cur->visited = visited;
            cur = state;
            visited1++;
        }
    }
    while (del != NULL)
    {
        cur = del->del;
        free(del);
        del = cur;
    }
}

static Fragment initFragment(State *state, State **next)
{
    Fragment fragment;
    fragment.state = state;
    fragment.next = next;
    return fragment;
}

static void freeFragment(Fragment *fragment, int visited, int visited1)
{
    freeState(fragment->state, visited, visited1);
    fragment->state = NULL;
    fragment->next = NULL;
}

static void patch(Fragment *fragment1, Fragment *fragment2)
{
    State *state = *fragment1->next;
    State *next = NULL;
    *fragment1->next = fragment2->state;
    for (; state != NULL; state = next)
    {
        next = state->out;
        state->out = fragment2->state;
    }
}

static void append(Fragment *fragment1, Fragment *fragment2)
{
    State *next = (State *)((char *)fragment2->next - ((char *)&fragment2->state->out - (char *)fragment2->state));
    State *state = *fragment1->next;
    if (NULL == state)
    {
        *fragment1->next = next;
    }
    else
    {
        while (state->out != NULL)
        {
            state = state->out;
        }
        state->out = next;
    }
}

static Fragment combineFragment(Fragment *fragment1, Fragment *fragment2)
{
    Fragment ret = initFragment(NULL, NULL);
    if (NULL == fragment1->state)
    {
        ret = *fragment2;
    }
    else
    {
        State *split = newState(SPLIT, fragment1->state, fragment2->state, NULL, -1);
        if (split != NULL)
        {
            append(fragment1, fragment2);
            ret = initFragment(split, fragment1->next);
        }
    }
    return ret;
}

static Fragment linkFragment(Fragment *fragment1, Fragment *fragment2)
{
    patch(fragment1, fragment2);
    Fragment ret = initFragment(fragment1->state, fragment2->next);
    return ret;
}

static int charToCharSet(CharSet *out, char c, int inverse, int backslash)
{
    if (backslash > 0)
    {
        switch (c)
        {
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '*':
        case '+':
        case '?':
        case '\\':
        case '|':
        case '.':
        case '^':
        case '$':
        case '-':
        {
            out->c[(unsigned char)c] = (char)!inverse;
            break;
        }
        case 'W':
        {
            CharSet charSet;
            initCharSet(&charSet, !inverse);
            for (char c = '0'; c <= '9'; c++)
            {
                charSet.c[(unsigned char)c] = (char)inverse;
            }

            for (char c = 'A'; c <= 'Z'; c++)
            {
                charSet.c[(unsigned char)c] = (char)inverse;
            }

            charSet.c['_'] = (char)inverse;

            for (char c = 'a'; c <= 'z'; c++)
            {
                charSet.c[(unsigned char)c] = (char)inverse;
            }

            for (size_t i = 0; i < STR_END; i++)
            {
                out->c[i] |= charSet.c[i];
            }
        }
        case 'w':
        {
            for (char c = '0'; c <= '9'; c++)
            {
                out->c[(unsigned char)c] = (char)!inverse;
            }

            for (char c = 'A'; c <= 'Z'; c++)
            {
                out->c[(unsigned char)c] = (char)!inverse;
            }

            out->c['_'] = (char)!inverse;

            for (char c = 'a'; c <= 'z'; c++)
            {
                out->c[(unsigned char)c] = (char)!inverse;
            }
            break;
        }
        case 'S':
        {
            CharSet charSet;
            initCharSet(&charSet, !inverse);
            charSet.c['\t'] = (char)inverse;
            charSet.c['\f'] = (char)inverse;
            charSet.c['\r'] = (char)inverse;
            charSet.c['\n'] = (char)inverse;
            charSet.c['\v'] = (char)inverse;
            charSet.c[' '] = (char)inverse;
            for (size_t i = 0; i < STR_END; i++)
            {
                out->c[i] |= charSet.c[i];
            }
        }
        case 's':
        {
            out->c['\t'] = (char)!inverse;
            out->c['\f'] = (char)!inverse;
            out->c['\r'] = (char)!inverse;
            out->c['\n'] = (char)!inverse;
            out->c['\v'] = (char)!inverse;
            out->c[' '] = (char)!inverse;
            break;
        }
        case 'D':
        {
            CharSet charSet;
            initCharSet(&charSet, !inverse);
            for (char c = '0'; c <= '9'; c++)
            {
                charSet.c[(unsigned char)c] = (char)inverse;
            }
            for (size_t i = 0; i < STR_END; i++)
            {
                out->c[i] |= charSet.c[i];
            }
        }
        case 'd':
        {
            for (char c = '0'; c <= '9'; c++)
            {
                out->c[(unsigned char)c] = (char)!inverse;
            }
            break;
        }
        case 'b':
        {
            out->c[WORD_MARGIN] = (char)!inverse;
            break;
        }
        default:
        {
            return -1;
        }
        }
    }
    else
    {
        switch (c)
        {
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '*':
        case '+':
        case '?':
        case '\\':
        case '|':
        case '.':
        case '^':
        case '$':
        {
            return -1;
        }
        default:
        {
            out->c[(unsigned char)c] = (char)!inverse;
        }
        }
    }
    return 0;
}

static Fragment combineCharByCharSet(const CharSet *charSet, int *groups, int deep, int visited, int visited1)
{
    Fragment ret = initFragment(NULL, NULL);
    State *state = NULL;
    for (size_t i = 1; i < CHAR_SET_SIZE; i++)
    {
        if (charSet->c[i] > 0)
        {
            state = newState(i, NULL, NULL, groups, deep);
            if (state != NULL)
            {
                Fragment fragment = initFragment(state, &state->out);
                ret = combineFragment(&ret, &fragment);
                if (NULL == ret.state)
                {
                    goto exception;
                }
            }
            else
            {
                goto exception;
            }
        }
    }
    goto finally;
exception:
    freeState(state, visited, visited1);
    freeFragment(&ret, visited, visited1);
finally:
    return ret;
}

static Fragment parseBackslash(char c, int *groups, int deep, int visited, int visited1)
{
    Fragment ret = initFragment(NULL, NULL);
    CharSet charSet;
    initCharSet(&charSet, 0);
    if (charToCharSet(&charSet, c, 0, 1) == 0)
    {
        ret = combineCharByCharSet(&charSet, groups, deep, visited, visited1);
    }
    return ret;
}

static int parseSquare(Fragment *out, const char *pattern, int *groups, int deep, int visited, int visited1)
{
    *out = initFragment(NULL, NULL);
    CharSet charSet;
    int i = 0;
    if ('^' == pattern[i])
    {
        i++;
        initCharSet(&charSet, 1);
        for (; pattern[i] != '\0' && pattern[i] != ']'; i++)
        {
            switch (pattern[i])
            {
            case '\\':
                i++;
                if (charToCharSet(&charSet, pattern[i], 1, 1) < 0)
                {
                    goto exception;
                }
                break;
            case '-':
                if (i > 1 && pattern[i + 1] != '\0' && pattern[i + 1] != ']')
                {
                    char begin = pattern[i - 1];
                    char end = pattern[i + 1];
                    for (char c = begin; c <= end; c++)
                    {
                        charSet.c[(unsigned char)c] = 0;
                    }
                    i++;
                }
                else
                {
                    goto exception;
                }
                break;
            default:
                if (charToCharSet(&charSet, pattern[i], 1, 0) < 0)
                {
                    goto exception;
                }
                break;
            }
        }
        *out = combineCharByCharSet(&charSet, groups, deep, visited, visited1);
        if (NULL == out->state)
        {
            goto exception;
        }
    }
    else
    {
        initCharSet(&charSet, 0);
        for (; pattern[i] != '\0' && pattern[i] != ']'; i++)
        {
            switch (pattern[i])
            {
            case '\\':
                i++;
                if (charToCharSet(&charSet, pattern[i], 0, 1) < 0)
                {
                    goto exception;
                }
                break;
            case '-':
                if (i > 0 && pattern[i + 1] != '\0' && pattern[i + 1] != ']')
                {
                    char begin = pattern[i - 1];
                    char end = pattern[i + 1];
                    for (char c = begin; c <= end; c++)
                    {
                        charSet.c[(unsigned char)c] = 1;
                    }
                    i++;
                }
                else
                {
                    goto exception;
                }
                break;
            default:
                if (charToCharSet(&charSet, pattern[i], 0, 0) < 0)
                {
                    goto exception;
                }
                break;
            }
        }
        *out = combineCharByCharSet(&charSet, groups, deep, visited, visited1);
        if (NULL == out->state)
        {
            goto exception;
        }
    }
    goto finally;
exception:
    i = -1;
finally:
    return i;
}

static int parseRepeat(Repeat *repeat, const char *pattern)
{
    int i = 0;
    repeat->min = 0;
    repeat->max = -1;
    if (pattern[i] < '0' || pattern[i] > '9')
    {
        return -1;
    }
    for (; pattern[i] != '\0' && pattern[i] != '}'; i++)
    {
        if (pattern[i] >= '0' && pattern[i] <= '9')
        {
            if (repeat->min != 0)
            {
                repeat->min *= 10;
            }
            repeat->min += pattern[i] - '0';
        }
        else if (',' == pattern[i])
        {
            repeat->max = 0;
            i++;
            break;
        }
        else
        {
            return -1;
        }
    }
    for (; pattern[i] != '\0' && pattern[i] != '}'; i++)
    {
        if (pattern[i] >= '0' && pattern[i] <= '9')
        {
            if (repeat->max != 0)
            {
                repeat->max *= 10;
            }
            repeat->max += pattern[i] - '0';
        }
        else
        {
            return -1;
        }
    }
    return i;
}

static State *copyState(State *state, int visited, int visited1)
{
    State *ret = NULL;
    if (state->visited != visited)
    {
        state->visited = visited;
        ret = newState(state->c, NULL, NULL, NULL, -1);
        if (NULL == ret)
        {
            goto exception;
        }
        state->copy = ret;
        for (size_t i = 0; i < MAX_GROUPS; i++)
        {
            ret->groups[i] = state->groups[i];
        }
        if (state->out != NULL)
        {
            if (state->out->visited != visited)
            {
                ret->out = copyState(state->out, visited, visited1);
                if (NULL == ret->out)
                {
                    goto exception;
                }
            }
            else
            {
                ret->out = state->out->copy;
            }
        }
        if (state->out1 != NULL)
        {
            if (state->out1->visited != visited)
            {
                ret->out1 = copyState(state->out1, visited, visited1);
                if (NULL == ret->out1)
                {
                    goto exception;
                }
            }
            else
            {
                ret->out1 = state->out1->copy;
            }
        }
    }
    goto finally;
exception:
    freeState(ret, visited, visited1);
    ret = NULL;
finally:
    return ret;
}

static Fragment copyFragment(Fragment *fragment, int visited, int visited1)
{
    Fragment ret;
    ret.state = copyState(fragment->state, visited, visited1);
    if (ret.state != NULL)
    {
        State *next = (State *)((char *)fragment->next - ((char *)&fragment->state->out - (char *)fragment->state));
        ret.next = &next->copy->out;
    }
    return ret;
}

static int parsePattern(Fragment *out, const char *pattern, int *paren, int *groups, int visited, int visited1)
{
    *out = initFragment(NULL, NULL);
    int square = 0;
    int brace = 0;
    FragmentStack stack;
    initFragmentStack(&stack);
    int i = 0;
    int or = 0;
    if (*paren >= 0 && *paren < MAX_GROUPS)
    {
        groups[*paren]++;
    }
    for (; pattern[i] != '\0'; i++)
    {
        switch (pattern[i])
        {
        case '(':
        {
            (*paren)++;
            Fragment fragment1;
            int result = parsePattern(&fragment1, &pattern[i + 1], paren, groups, visited, visited1);
            if (result >= 0)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                if (fragment1.state != NULL)
                {
                    pushFragmentStack(&stack, &fragment1);
                }
                i += result + 1;
            }
            else
            {
                goto exception;
            }
            break;
        }
        case ')':
        {
            (*paren)--;
            goto out;
        }
        case '[':
        {
            square++;
            Fragment fragment1;
            int result = parseSquare(&fragment1, &pattern[i + 1], groups, *paren, visited, visited1);
            if (result >= 0)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                pushFragmentStack(&stack, &fragment1);
                i += result;
            }
            else
            {
                goto exception;
            }
            break;
        }
        case ']':
        {
            square--;
            break;
        }
        case '{':
        {
            brace++;
            Repeat repeat;
            int result = parseRepeat(&repeat, &pattern[i + 1]);
            if (result >= 0)
            {
                if (repeat.max > 0 && repeat.max < repeat.min)
                {
                    goto exception;
                }
                if (stack.n > 0 + or)
                {
                    Fragment fragment = popFragmentStack(&stack);
                    for (size_t i = 0; i < repeat.min; i++)
                    {
                        if (stack.n > 1 + or)
                        {
                            Fragment fragment2 = popFragmentStack(&stack);
                            Fragment fragment1 = popFragmentStack(&stack);
                            Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                            pushFragmentStack(&stack, &fragment3);
                        }
                        Fragment fragment1 = copyFragment(&fragment, visited++, visited1++);
                        if (NULL == fragment1.state)
                        {
                            freeFragment(&fragment, visited++, visited1++);
                            goto exception;
                        }
                        pushFragmentStack(&stack, &fragment1);
                    }
                    if (0 == repeat.max)
                    {
                        if (stack.n > 1 + or)
                        {
                            Fragment fragment2 = popFragmentStack(&stack);
                            Fragment fragment1 = popFragmentStack(&stack);
                            Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                            pushFragmentStack(&stack, &fragment3);
                        }
                        Fragment fragment1 = copyFragment(&fragment, visited++, visited1++);
                        if (NULL == fragment1.state)
                        {
                            freeFragment(&fragment, visited++, visited1++);
                            goto exception;
                        }
                        State *state = newState(SPLIT, NULL, fragment1.state, NULL, -1);
                        if (state != NULL)
                        {
                            Fragment fragment2 = initFragment(state, &state->out);
                            patch(&fragment1, &fragment2);
                            pushFragmentStack(&stack, &fragment2);
                        }
                        else
                        {
                            freeFragment(&fragment1, visited++, visited1++);
                            freeFragment(&fragment, visited++, visited1++);
                            goto exception;
                        }
                    }
                    else if (repeat.max > 0)
                    {
                        for (size_t i = 0; i < repeat.max - repeat.min; i++)
                        {
                            if (stack.n > 1 + or)
                            {
                                Fragment fragment2 = popFragmentStack(&stack);
                                Fragment fragment1 = popFragmentStack(&stack);
                                Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                                pushFragmentStack(&stack, &fragment3);
                            }
                            Fragment fragment1 = copyFragment(&fragment, visited++, visited1++);
                            if (NULL == fragment1.state)
                            {
                                freeFragment(&fragment, visited++, visited1++);
                                goto exception;
                            }
                            State *state = newState(SPLIT, NULL, fragment1.state, NULL, -1);
                            if (state != NULL)
                            {
                                Fragment fragment2 = initFragment(state, &state->out);
                                append(&fragment1, &fragment2);
                                fragment2 = initFragment(state, fragment1.next);
                                pushFragmentStack(&stack, &fragment2);
                            }
                            else
                            {
                                freeFragment(&fragment1, visited++, visited1++);
                                freeFragment(&fragment, visited++, visited1++);
                                goto exception;
                            }
                        }
                    }
                    freeFragment(&fragment, visited++, visited1++);
                }
                else
                {
                    goto exception;
                }
                i += result;
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '}':
        {
            brace--;
            break;
        }
        case '*':
        {
            if (stack.n > 0 + or)
            {
                Fragment fragment = popFragmentStack(&stack);
                State *state = newState(SPLIT, NULL, fragment.state, NULL, -1);
                if (state != NULL)
                {
                    Fragment fragment1 = initFragment(state, &state->out);
                    patch(&fragment, &fragment1);
                    pushFragmentStack(&stack, &fragment1);
                }
                else
                {
                    freeFragment(&fragment, visited, visited1);
                    goto exception;
                }
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '+':
        {
            if (stack.n > 0 + or)
            {
                Fragment fragment = popFragmentStack(&stack);
                State *state = newState(SPLIT, NULL, fragment.state, NULL, -1);
                if (state != NULL)
                {
                    Fragment fragment1 = initFragment(state, &state->out);
                    patch(&fragment, &fragment1);
                    fragment1 = initFragment(fragment.state, &state->out);
                    pushFragmentStack(&stack, &fragment1);
                }
                else
                {
                    freeFragment(&fragment, visited, visited1);
                    goto exception;
                }
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '?':
        {
            if (stack.n > 0 + or)
            {
                Fragment fragment = popFragmentStack(&stack);
                State *state = newState(SPLIT, NULL, fragment.state, NULL, -1);
                if (state != NULL)
                {
                    Fragment fragment1 = initFragment(state, &state->out);
                    append(&fragment, &fragment1);
                    fragment1 = initFragment(state, fragment.next);
                    pushFragmentStack(&stack, &fragment1);
                }
                else
                {
                    freeFragment(&fragment, visited, visited1);
                    goto exception;
                }
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '|':
        {
            if (stack.n > 0)
            {
                if (or > 0)
                {
                    Fragment fragment3;
                    if (stack.n > 2)
                    {
                        Fragment fragment2 = popFragmentStack(&stack);
                        Fragment fragment1 = popFragmentStack(&stack);
                        fragment3 = linkFragment(&fragment1, &fragment2);
                    }
                    else if (stack.n > 1)
                    {
                        fragment3 = popFragmentStack(&stack);
                    }
                    else
                    {
                        goto exception;
                    }
                    Fragment fragment  = popFragmentStack(&stack);
                    Fragment fragment1 = combineFragment(&fragment, &fragment3);
                    if (fragment1.state != NULL)
                    {
                        pushFragmentStack(&stack, &fragment1);
                    }
                    else
                    {
                        freeFragment(&fragment3, visited, visited1);
                        freeFragment(&fragment, visited, visited1);
                        goto exception;
                    }
                }
                else
                {
                    if (stack.n > 1)
                    {
                        Fragment fragment2 = popFragmentStack(&stack);
                        Fragment fragment1 = popFragmentStack(&stack);
                        Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                        pushFragmentStack(&stack, &fragment3);
                    }
                    or++;
                }
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '\\':
        {
            Fragment fragment1 = parseBackslash(pattern[i + 1], groups, *paren, visited, visited1);
            if (fragment1.state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                pushFragmentStack(&stack, &fragment1);
                i++;
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '.':
        {
            CharSet charSet;
            initCharSet(&charSet, 1);
            charToCharSet(&charSet, '\n', 1, 0);
            Fragment fragment1 = combineCharByCharSet(&charSet, groups, *paren, visited, visited1);
            if (fragment1.state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                pushFragmentStack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '^':
        {
            State *state = newState(STR_START, NULL, NULL, NULL, -1);
            if (state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                Fragment fragment1 = initFragment(state, &state->out);
                pushFragmentStack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '$':
        {
            State *state = newState(STR_END, NULL, NULL, NULL, -1);
            if (state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                Fragment fragment1 = initFragment(state, &state->out);
                pushFragmentStack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        default:
        {
            State *state = newState((unsigned char)pattern[i], NULL, NULL, groups, *paren);
            if (state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    Fragment fragment2 = popFragmentStack(&stack);
                    Fragment fragment1 = popFragmentStack(&stack);
                    Fragment fragment3 = linkFragment(&fragment1, &fragment2);
                    pushFragmentStack(&stack, &fragment3);
                }
                Fragment fragment1 = initFragment(state, &state->out);
                pushFragmentStack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        }
    }
out:
    if (square != 0 || brace != 0)
    {
        goto exception;
    }
    Fragment fragment3 = {NULL, NULL};
    if (stack.n > 1 + or)
    {
        Fragment fragment2 = popFragmentStack(&stack);
        Fragment fragment1 = popFragmentStack(&stack);
        fragment3 = linkFragment(&fragment1, &fragment2);
    }
    else if (stack.n > 0 + or)
    {
        fragment3 = popFragmentStack(&stack);
    }
    if (or > 0)
    {
        if (stack.n > 0)
        {
            Fragment fragment = popFragmentStack(&stack);
            *out = combineFragment(&fragment, &fragment3);
            if (NULL == out->state)
            {
                freeFragment(&fragment, visited, visited1);
                goto exception;
            }
        }
        else
        {
            goto exception;
        }
    }
    else
    {
        *out = fragment3;
    }
    goto finally;
exception:
    i = -1;
    while (stack.n > 0)
    {
        Fragment fragment = popFragmentStack(&stack);
        freeFragment(&fragment, visited, visited1);
    }
finally:
    return i;
}

static void traverseStates(StateSet *stateSet, State *state, int visited)
{
    if (state->visited != visited)
    {
        state->visited = visited;
        if (state->c != SPLIT)
        {
            stateSet->states[stateSet->count++] = state;
        }
        else
        {
            if (state->out != NULL)
            {
                traverseStates(stateSet, state->out, visited);
            }
            if (state->out1 != NULL)
            {
                traverseStates(stateSet, state->out1, visited);
            }
        }
    }
}

static StateSet newStateSet(size_t size)
{
    StateSet stateSet = {NULL, 0, NULL};
    stateSet.states = (State **)malloc(size * sizeof(State *));
    stateSet.count = 0;
    return stateSet;
}

static void freeStateSet(StateSet *stateSet)
{
    if (stateSet->states != NULL)
    {
        free(stateSet->states);
        stateSet->states = NULL;
        stateSet->count = 0;
    }
}

static void nextStates(StateSet *out, StateSet *states, int visited)
{
    for (size_t i = 0; i < states->count; i++)
    {
        if (states->states[i]->out != NULL)
        {
            traverseStates(out, states->states[i]->out, visited);
        }
    }
    
}

static const unsigned char IS_WORD[] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16-31
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 32-47
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 48-63
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 64-79
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 80-95
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 96-111
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 112-127
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 128-143
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 144-159
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 160-175
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 176-191
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 192-207
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 208-223
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 224-239
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 240-255
};

static int isWordMargin(const char *str, size_t i)
{
    if (i > 0)
    {
        if ((IS_WORD[(unsigned char)str[i]] > 0 && IS_WORD[(unsigned char)str[i - 1]] == 0) ||
        (IS_WORD[(unsigned char)str[i]] == 0 && IS_WORD[(unsigned char)str[i - 1]] > 0))
        {
            return 1;
        }
    }
    else
    {
        return IS_WORD[(unsigned char)str[i]];
    }
    return 0;
}

static int isStrStart(const char *str, size_t i)
{
    if (str[i] != '\0')
    {
        if (i > 0)
        {
            if ('\n' == str[i - 1])
            {
                return 1;
            }
        }
        else
        {
            return 1;
        }
    }
    return 0;
}

static int isStrEnd(const char *str, size_t i)
{
    if ('\0' == str[i] || '\n' == str[i])
    {
        return 1;
    }
    return 0;
}

static StateSet *copyNewStateSet(const StateSet *old)
{
    StateSet *stateSet = (StateSet *)malloc(sizeof(StateSet));
    if (NULL == stateSet)
    {
        return NULL;
    }
    *stateSet = newStateSet(old->count);
    if (NULL == stateSet->states)
    {
        free(stateSet);
        return NULL;
    }
    for (size_t i = 0; i < old->count; i++)
    {
        stateSet->states[i] = old->states[i];
    }
    stateSet->count = old->count;
    return stateSet;
}

static void freeAllStateSet(StateSet *stateSet)
{
    while (stateSet != NULL)
    {
        StateSet *next = stateSet->next;
        free(stateSet->states);
        free(stateSet);
        stateSet = next;
    }
}

static int checkPreState(State *pre, const State *next, int visited)
{
    if (pre != NULL && pre->visited != visited)
    {
        pre->visited = visited;
        switch (pre->c)
        {
        case MATCH:
            break;
        case SPLIT:
            switch (pre->out->c)
            {
            case SPLIT:
            case WORD_MARGIN:
            case STR_START:
            case STR_END:
                if (checkPreState(pre->out, next, visited) == 0)
                {
                    return 0;
                }
                break;
            default:
                if (pre->out == next)
                {
                    return 0;
                }
                break;
            }
            switch (pre->out1->c)
            {
            case SPLIT:
            case WORD_MARGIN:
            case STR_START:
            case STR_END:
                if (checkPreState(pre->out1, next, visited) == 0)
                {
                    return 0;
                }
                break;
            default:
                if (pre->out1 == next)
                {
                    return 0;
                }
                break;
            }
            break;
        case WORD_MARGIN:
        case STR_START:
        case STR_END:
        default:
            switch (pre->out->c)
            {
            case SPLIT:
            case WORD_MARGIN:
            case STR_START:
            case STR_END:
                if (checkPreState(pre->out, next, visited) == 0)
                {
                    return 0;
                }
                break;
            default:
                if (pre->out == next)
                {
                    return 0;
                }
                break;
            }
            break;
        }
    }
    return -1;
}

static State *findPreState(const StateSet *stateSet, const State *state, int visited)
{
    State *ret = NULL;
    for (size_t i = 0; i < stateSet->count; i++)
    {
        if (checkPreState(stateSet->states[i], state, visited) == 0)
        {
            ret = stateSet->states[i];
            break;
        }
    }
    return ret;
}

static void initRegexMatch(CRegexMatch *match)
{
    match->begin = (size_t)-1;
    match->len = 0;
}

static int match(CRegex *regex, const char *text, CRegexMatch *matchs, size_t nMatch, int flag)
{   
    int ret = -1;
    StateSet *root = NULL;
    for (size_t i = 0; i < nMatch; i++)
    {
        initRegexMatch(&matchs[i]);
    }
    regex->next.count = 0;
    regex->visited++;
    traverseStates(&regex->next, regex->root, regex->visited);
    for (size_t i = 0;; i++)
    {
        State *match = NULL;
        regex->cmp.count = 0;
        for (size_t j = 0; j < regex->next.count; j++)
        {
            int c = regex->next.states[j]->c;
            switch (c)
            {
            case MATCH:
                match = regex->next.states[j];
                break;
            case WORD_MARGIN:
                if (isWordMargin(text, i) > 0)
                {
                    traverseStates(&regex->next, regex->next.states[j]->out, regex->visited);
                }
                break;
            case STR_START:
                if (isStrStart(text, i) > 0)
                {
                    traverseStates(&regex->next, regex->next.states[j]->out, regex->visited);
                }
                break;
            case STR_END:
                if (isStrEnd(text, i) > 0)
                {
                    traverseStates(&regex->next, regex->next.states[j]->out, regex->visited);
                }
                break;
            default:
                if (c == (unsigned char)text[i])
                {
                    regex->cmp.states[regex->cmp.count++] = regex->next.states[j];
                }
                break;
            }
        }
        if (regex->cmp.count > 0)
        {
            regex->next.count = 0;
            regex->visited++;
            nextStates(&regex->next, &regex->cmp, regex->visited);
            if ((flag & CREGEX_FLAG_NOSUB) != CREGEX_FLAG_NOSUB && nMatch > 1)
            {
                StateSet *stateSet = copyNewStateSet(&regex->cmp);
                if (NULL == stateSet)
                {
                    goto exception;
                }
                if (NULL == root)
                {
                    root = stateSet;
                }
                else
                {
                    stateSet->next = root;
                    root = stateSet;
                }
            }
        }
        else if (match != NULL)
        {
            ret = (int)i;
            if (nMatch > 0)
            {
                matchs[0].begin = regex->index;
                matchs[0].len = i;
            }
            StateSet *cur = root;
            for (; cur != NULL; i--)
            {
                regex->visited++;
                match = findPreState(cur, match, regex->visited);
                for (int j = 0; j < MAX_GROUPS; j++)
                {
                    if (match->groups[j] >= 0)
                    {
                        int index = 0;
                        for (int k = j - 1; k >= 0; k--)
                        {
                            index += regex->groups[k];
                        }
                        index += match->groups[j];
                        if (index + 1 < nMatch)
                        {
                            matchs[index + 1].begin = regex->index + i - 1;
                            matchs[index + 1].len++;
                        }
                        
                    }
                }
                cur = cur->next;
            }
            goto finally;
        }
        else
        {
            goto exception;
        }
        if ('\0' == text[i])
        {
            break;
        }
    }
exception:
finally:
    freeAllStateSet(root);
    return ret;
}

CRegex *cRegexCompile(const char *pattern)
{
    CRegex *regex = malloc(sizeof(CRegex));
    if (NULL == regex)
    {
        goto exception;
    }
    regex->index = 0;
    size_t len = strlen(pattern);
    regex->cmp = newStateSet(256 * len);
    if (NULL == regex->cmp.states)
    {
        goto exception;
    }
    regex->next = newStateSet(256 * len);
    if (NULL == regex->next.states)
    {
        goto exception;
    }
    Fragment fragment = initFragment(NULL, NULL);
    int paren = -1;
    for (size_t i = 0; i < MAX_GROUPS; i++)
    {
        regex->groups[i] = 0;
    }
    regex->visited = 1;
    regex->visited1 = 1;
    int result = parsePattern(&fragment, pattern, &paren, regex->groups, regex->visited, regex->visited1);
    if (result < 0 || paren != -1)
    {
        goto exception;
    }
    regex->matchState.c = MATCH;
    regex->matchState.out = NULL;
    regex->matchState.out1 = NULL;
    regex->matchState.copy = NULL;
    regex->matchState.visited = 0;
    regex->matchState.visited1 = 0;
    regex->matchState.del = NULL;
    Fragment match = initFragment(&regex->matchState, NULL);
    if (fragment.state != NULL)
    {
        patch(&fragment, &match);
        regex->root = fragment.state;
    }
    else
    {
        regex->root = match.state;
    }
    goto finally;
exception:
    regex->visited++;
    regex->visited1++;
    freeFragment(&fragment, regex->visited, regex->visited1);
    freeStateSet(&regex->cmp);
    freeStateSet(&regex->next);
    if (regex != NULL)
    {
        free(regex);
    }
    return NULL;
finally:
    return regex;
}

void cRegexFree(CRegex *regex)
{
    if (regex->root != NULL)
    {
        regex->visited++;
        regex->matchState.visited = regex->visited;
        regex->visited1++;
        freeState(regex->root, regex->visited, regex->visited1);
        freeStateSet(&regex->next);
        freeStateSet(&regex->cmp);
        free(regex);
    }
}

int cRegexMatch(CRegex *regex, const char *text, CRegexMatch *matchs, size_t nMatch, int flag)
{
    int len = match(regex, text, matchs, nMatch, flag);
    if (len >= 0 && '\0' == text[len])
    {
        return 0;
    }
    return -1;
}

int cRegexSearch(CRegex *regex, const char *text, CRegexMatch *matchs, size_t nMatch, int flag)
{
    while (text[regex->index] != '\0')
    {
        int len = match(regex, &text[regex->index], matchs, nMatch, flag);
        if (len > 0)
        {
            regex->index += len;
            return 0;
        }
        else
        {
            regex->index++;
        }
    }
    return -1;
}