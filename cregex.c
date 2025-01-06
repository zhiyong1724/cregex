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

typedef struct repeat_t
{
    int min;
    int max;
} repeat_t;

#define MAX_GROUPS 8
typedef struct state_t
{
    int c;
    struct state_t *out;
    struct state_t *out1;
    struct state_t *copy;
    int groups[MAX_GROUPS];
    int visited;
    int visited1;
    struct state_t *del;
} state_t;

typedef struct fragment_t
{
    state_t *state;
    state_t **next;
} fragment_t;

typedef struct fragment_stack_t
{
    fragment_t stack[3];
    size_t n;
} fragment_stack_t;

typedef struct char_set_t
{
    char c[CHAR_SET_SIZE];
} char_set_t;

typedef struct state_set_t
{
    state_t **states;
    size_t count;
    struct state_set_t *next;
} state_set_t;

struct cregex_t
{
    state_t match_state;
    state_t *root;
    state_set_t next;
    state_set_t cmp;
    int groups[MAX_GROUPS];
    int index;
    int visited;
    int visited1;
};

static void init_char_set(char_set_t *char_set, int inverse)
{
    memset(char_set, inverse, CHAR_SET_SIZE);
    if (inverse > 0)
    {
        char_set->c[WORD_MARGIN] = 0;
        char_set->c[STR_START] = 0;
        char_set->c[STR_END] = 0;
    }
}

static void init_fragment_stack(fragment_stack_t *stack)
{
    stack->n = 0;
}

static void push_fragment_stack(fragment_stack_t *stack, fragment_t *fragment)
{
    stack->stack[stack->n++] = *fragment;
}

static fragment_t pop_fragment_stack(fragment_stack_t *stack)
{
    return stack->stack[--stack->n];
}

static state_t *new_state(int c, state_t *out, state_t *out1, int *groups, int deep)
{
    state_t *state = (state_t *)malloc(sizeof(state_t));
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

static void free_state(state_t *state, int visited, int visited1)
{
    state_t *del = NULL;
    state_t *last = NULL;
    state_t *cur = state;
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

static fragment_t init_fragment(state_t *state, state_t **next)
{
    fragment_t fragment;
    fragment.state = state;
    fragment.next = next;
    return fragment;
}

static void free_fragment(fragment_t *fragment, int visited, int visited1)
{
    free_state(fragment->state, visited, visited1);
    fragment->state = NULL;
    fragment->next = NULL;
}

static void patch(fragment_t *fragment1, fragment_t *fragment2)
{
    state_t *state = *fragment1->next;
    state_t *next = NULL;
    *fragment1->next = fragment2->state;
    for (; state != NULL; state = next)
    {
        next = state->out;
        state->out = fragment2->state;
    }
}

static void append(fragment_t *fragment1, fragment_t *fragment2)
{
    state_t *next = (state_t *)((char *)fragment2->next - ((char *)&fragment2->state->out - (char *)fragment2->state));
    state_t *state = *fragment1->next;
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

static fragment_t combine_fragment(fragment_t *fragment1, fragment_t *fragment2)
{
    fragment_t ret = init_fragment(NULL, NULL);
    if (NULL == fragment1->state)
    {
        ret = *fragment2;
    }
    else
    {
        state_t *split = new_state(SPLIT, fragment1->state, fragment2->state, NULL, -1);
        if (split != NULL)
        {
            append(fragment1, fragment2);
            ret = init_fragment(split, fragment1->next);
        }
    }
    return ret;
}

static fragment_t link_fragment(fragment_t *fragment1, fragment_t *fragment2)
{
    patch(fragment1, fragment2);
    fragment_t ret = init_fragment(fragment1->state, fragment2->next);
    return ret;
}

static int char_to_char_set(char_set_t *out, char c, int inverse, int backslash, int for_square)
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
        {
            out->c[(unsigned char)c] = (char)!inverse;
            break;
        }
        case '-':
        {
            if (for_square > 0)
            {
                out->c[(unsigned char)c] = (char)!inverse;
            }
            else
            {
                return -1;
            }
            break;
        }
        case 'W':
        {
            char_set_t char_set;
            init_char_set(&char_set, !inverse);
            for (char c = '0'; c <= '9'; c++)
            {
                char_set.c[(unsigned char)c] = (char)inverse;
            }

            for (char c = 'A'; c <= 'Z'; c++)
            {
                char_set.c[(unsigned char)c] = (char)inverse;
            }

            char_set.c['_'] = (char)inverse;

            for (char c = 'a'; c <= 'z'; c++)
            {
                char_set.c[(unsigned char)c] = (char)inverse;
            }

            for (size_t i = 0; i < STR_END; i++)
            {
                out->c[i] |= char_set.c[i];
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
            char_set_t char_set;
            init_char_set(&char_set, !inverse);
            char_set.c['\t'] = (char)inverse;
            char_set.c['\f'] = (char)inverse;
            char_set.c['\r'] = (char)inverse;
            char_set.c['\n'] = (char)inverse;
            char_set.c['\v'] = (char)inverse;
            char_set.c[' '] = (char)inverse;
            for (size_t i = 0; i < STR_END; i++)
            {
                out->c[i] |= char_set.c[i];
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
            char_set_t char_set;
            init_char_set(&char_set, !inverse);
            for (char c = '0'; c <= '9'; c++)
            {
                char_set.c[(unsigned char)c] = (char)inverse;
            }
            for (size_t i = 0; i < STR_END; i++)
            {
                out->c[i] |= char_set.c[i];
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
            if (for_square > 0)
            {
                out->c[(unsigned char)c] = (char)!inverse;
            }
            else
            {
                return -1;
            }
            break;
        }
        default:
        {
            out->c[(unsigned char)c] = (char)!inverse;
        }
        }
    }
    return 0;
}

static fragment_t combine_char_by_char_set(const char_set_t *char_set, int *groups, int deep, int visited, int visited1)
{
    fragment_t ret = init_fragment(NULL, NULL);
    state_t *state = NULL;
    for (size_t i = 1; i < CHAR_SET_SIZE; i++)
    {
        if (char_set->c[i] > 0)
        {
            state = new_state(i, NULL, NULL, groups, deep);
            if (state != NULL)
            {
                fragment_t fragment = init_fragment(state, &state->out);
                ret = combine_fragment(&ret, &fragment);
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
    free_state(state, visited, visited1);
    free_fragment(&ret, visited, visited1);
finally:
    return ret;
}

static fragment_t parse_backslash(char c, int *groups, int deep, int visited, int visited1)
{
    fragment_t ret = init_fragment(NULL, NULL);
    char_set_t char_set;
    init_char_set(&char_set, 0);
    if (char_to_char_set(&char_set, c, 0, 1, 0) == 0)
    {
        ret = combine_char_by_char_set(&char_set, groups, deep, visited, visited1);
    }
    return ret;
}

static int parse_square(fragment_t *out, const char *pattern, int *groups, int deep, int visited, int visited1)
{
    *out = init_fragment(NULL, NULL);
    char_set_t char_set;
    int i = 0;
    if ('^' == pattern[i])
    {
        i++;
        init_char_set(&char_set, 1);
        for (; pattern[i] != '\0' && pattern[i] != ']'; i++)
        {
            switch (pattern[i])
            {
            case '\\':
                i++;
                if (char_to_char_set(&char_set, pattern[i], 1, 1, 1) < 0)
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
                        char_set.c[(unsigned char)c] = 0;
                    }
                    i++;
                }
                else
                {
                    goto exception;
                }
                break;
            default:
                if (char_to_char_set(&char_set, pattern[i], 1, 0, 1) < 0)
                {
                    goto exception;
                }
                break;
            }
        }
        *out = combine_char_by_char_set(&char_set, groups, deep, visited, visited1);
        if (NULL == out->state)
        {
            goto exception;
        }
    }
    else
    {
        init_char_set(&char_set, 0);
        for (; pattern[i] != '\0' && pattern[i] != ']'; i++)
        {
            switch (pattern[i])
            {
            case '\\':
                i++;
                if (char_to_char_set(&char_set, pattern[i], 0, 1, 1) < 0)
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
                        char_set.c[(unsigned char)c] = 1;
                    }
                    i++;
                }
                else
                {
                    goto exception;
                }
                break;
            default:
                if (char_to_char_set(&char_set, pattern[i], 0, 0, 1) < 0)
                {
                    goto exception;
                }
                break;
            }
        }
        *out = combine_char_by_char_set(&char_set, groups, deep, visited, visited1);
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

static int parse_repeat(repeat_t *repeat, const char *pattern)
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

static state_t *copy_state(state_t *state, int visited, int visited1)
{
    state_t *ret = NULL;
    if (state->visited != visited)
    {
        state->visited = visited;
        ret = new_state(state->c, NULL, NULL, NULL, -1);
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
                ret->out = copy_state(state->out, visited, visited1);
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
                ret->out1 = copy_state(state->out1, visited, visited1);
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
    free_state(ret, visited, visited1);
    ret = NULL;
finally:
    return ret;
}

static fragment_t copy_fragment(fragment_t *fragment, int visited, int visited1)
{
    fragment_t ret;
    ret.state = copy_state(fragment->state, visited, visited1);
    if (ret.state != NULL)
    {
        state_t *next = (state_t *)((char *)fragment->next - ((char *)&fragment->state->out - (char *)fragment->state));
        ret.next = &next->copy->out;
    }
    return ret;
}

static int parse_pattern(fragment_t *out, const char *pattern, int *paren, int *groups, int visited, int visited1)
{
    *out = init_fragment(NULL, NULL);
    int square = 0;
    int brace = 0;
    fragment_stack_t stack;
    init_fragment_stack(&stack);
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
            fragment_t fragment1;
            int result = parse_pattern(&fragment1, &pattern[i + 1], paren, groups, visited, visited1);
            if (result >= 0)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                if (fragment1.state != NULL)
                {
                    push_fragment_stack(&stack, &fragment1);
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
            fragment_t fragment1;
            int result = parse_square(&fragment1, &pattern[i + 1], groups, *paren, visited, visited1);
            if (result >= 0)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                push_fragment_stack(&stack, &fragment1);
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
            repeat_t repeat;
            int result = parse_repeat(&repeat, &pattern[i + 1]);
            if (result >= 0)
            {
                if (repeat.max > 0 && repeat.max < repeat.min)
                {
                    goto exception;
                }
                if (stack.n > 0 + or)
                {
                    fragment_t fragment = pop_fragment_stack(&stack);
                    for (size_t i = 0; i < repeat.min; i++)
                    {
                        if (stack.n > 1 + or)
                        {
                            fragment_t fragment2 = pop_fragment_stack(&stack);
                            fragment_t fragment1 = pop_fragment_stack(&stack);
                            fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                            push_fragment_stack(&stack, &fragment3);
                        }
                        fragment_t fragment1 = copy_fragment(&fragment, visited++, visited1++);
                        if (NULL == fragment1.state)
                        {
                            free_fragment(&fragment, visited++, visited1++);
                            goto exception;
                        }
                        push_fragment_stack(&stack, &fragment1);
                    }
                    if (0 == repeat.max)
                    {
                        if (stack.n > 1 + or)
                        {
                            fragment_t fragment2 = pop_fragment_stack(&stack);
                            fragment_t fragment1 = pop_fragment_stack(&stack);
                            fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                            push_fragment_stack(&stack, &fragment3);
                        }
                        fragment_t fragment1 = copy_fragment(&fragment, visited++, visited1++);
                        if (NULL == fragment1.state)
                        {
                            free_fragment(&fragment, visited++, visited1++);
                            goto exception;
                        }
                        state_t *state = new_state(SPLIT, NULL, fragment1.state, NULL, -1);
                        if (state != NULL)
                        {
                            fragment_t fragment2 = init_fragment(state, &state->out);
                            patch(&fragment1, &fragment2);
                            push_fragment_stack(&stack, &fragment2);
                        }
                        else
                        {
                            free_fragment(&fragment1, visited++, visited1++);
                            free_fragment(&fragment, visited++, visited1++);
                            goto exception;
                        }
                    }
                    else if (repeat.max > 0)
                    {
                        for (size_t i = 0; i < repeat.max - repeat.min; i++)
                        {
                            if (stack.n > 1 + or)
                            {
                                fragment_t fragment2 = pop_fragment_stack(&stack);
                                fragment_t fragment1 = pop_fragment_stack(&stack);
                                fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                                push_fragment_stack(&stack, &fragment3);
                            }
                            fragment_t fragment1 = copy_fragment(&fragment, visited++, visited1++);
                            if (NULL == fragment1.state)
                            {
                                free_fragment(&fragment, visited++, visited1++);
                                goto exception;
                            }
                            state_t *state = new_state(SPLIT, NULL, fragment1.state, NULL, -1);
                            if (state != NULL)
                            {
                                fragment_t fragment2 = init_fragment(state, &state->out);
                                append(&fragment1, &fragment2);
                                fragment2 = init_fragment(state, fragment1.next);
                                push_fragment_stack(&stack, &fragment2);
                            }
                            else
                            {
                                free_fragment(&fragment1, visited++, visited1++);
                                free_fragment(&fragment, visited++, visited1++);
                                goto exception;
                            }
                        }
                    }
                    free_fragment(&fragment, visited++, visited1++);
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
                fragment_t fragment = pop_fragment_stack(&stack);
                state_t *state = new_state(SPLIT, NULL, fragment.state, NULL, -1);
                if (state != NULL)
                {
                    fragment_t fragment1 = init_fragment(state, &state->out);
                    patch(&fragment, &fragment1);
                    push_fragment_stack(&stack, &fragment1);
                }
                else
                {
                    free_fragment(&fragment, visited, visited1);
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
                fragment_t fragment = pop_fragment_stack(&stack);
                state_t *state = new_state(SPLIT, NULL, fragment.state, NULL, -1);
                if (state != NULL)
                {
                    fragment_t fragment1 = init_fragment(state, &state->out);
                    patch(&fragment, &fragment1);
                    fragment1 = init_fragment(fragment.state, &state->out);
                    push_fragment_stack(&stack, &fragment1);
                }
                else
                {
                    free_fragment(&fragment, visited, visited1);
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
                fragment_t fragment = pop_fragment_stack(&stack);
                state_t *state = new_state(SPLIT, NULL, fragment.state, NULL, -1);
                if (state != NULL)
                {
                    fragment_t fragment1 = init_fragment(state, &state->out);
                    append(&fragment, &fragment1);
                    fragment1 = init_fragment(state, fragment.next);
                    push_fragment_stack(&stack, &fragment1);
                }
                else
                {
                    free_fragment(&fragment, visited, visited1);
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
                    fragment_t fragment3;
                    if (stack.n > 2)
                    {
                        fragment_t fragment2 = pop_fragment_stack(&stack);
                        fragment_t fragment1 = pop_fragment_stack(&stack);
                        fragment3 = link_fragment(&fragment1, &fragment2);
                    }
                    else if (stack.n > 1)
                    {
                        fragment3 = pop_fragment_stack(&stack);
                    }
                    else
                    {
                        goto exception;
                    }
                    fragment_t fragment  = pop_fragment_stack(&stack);
                    fragment_t fragment1 = combine_fragment(&fragment, &fragment3);
                    if (fragment1.state != NULL)
                    {
                        push_fragment_stack(&stack, &fragment1);
                    }
                    else
                    {
                        free_fragment(&fragment3, visited, visited1);
                        free_fragment(&fragment, visited, visited1);
                        goto exception;
                    }
                }
                else
                {
                    if (stack.n > 1)
                    {
                        fragment_t fragment2 = pop_fragment_stack(&stack);
                        fragment_t fragment1 = pop_fragment_stack(&stack);
                        fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                        push_fragment_stack(&stack, &fragment3);
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
            fragment_t fragment1 = parse_backslash(pattern[i + 1], groups, *paren, visited, visited1);
            if (fragment1.state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                push_fragment_stack(&stack, &fragment1);
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
            char_set_t char_set;
            init_char_set(&char_set, 1);
            char_to_char_set(&char_set, '\n', 1, 0, 0);
            fragment_t fragment1 = combine_char_by_char_set(&char_set, groups, *paren, visited, visited1);
            if (fragment1.state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                push_fragment_stack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '^':
        {
            state_t *state = new_state(STR_START, NULL, NULL, NULL, -1);
            if (state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                fragment_t fragment1 = init_fragment(state, &state->out);
                push_fragment_stack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        case '$':
        {
            state_t *state = new_state(STR_END, NULL, NULL, NULL, -1);
            if (state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                fragment_t fragment1 = init_fragment(state, &state->out);
                push_fragment_stack(&stack, &fragment1);
            }
            else
            {
                goto exception;
            }
            break;
        }
        default:
        {
            state_t *state = new_state((unsigned char)pattern[i], NULL, NULL, groups, *paren);
            if (state != NULL)
            {
                if (stack.n > 1 + or)
                {
                    fragment_t fragment2 = pop_fragment_stack(&stack);
                    fragment_t fragment1 = pop_fragment_stack(&stack);
                    fragment_t fragment3 = link_fragment(&fragment1, &fragment2);
                    push_fragment_stack(&stack, &fragment3);
                }
                fragment_t fragment1 = init_fragment(state, &state->out);
                push_fragment_stack(&stack, &fragment1);
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
    fragment_t fragment3 = {NULL, NULL};
    if (stack.n > 1 + or)
    {
        fragment_t fragment2 = pop_fragment_stack(&stack);
        fragment_t fragment1 = pop_fragment_stack(&stack);
        fragment3 = link_fragment(&fragment1, &fragment2);
    }
    else if (stack.n > 0 + or)
    {
        fragment3 = pop_fragment_stack(&stack);
    }
    if (or > 0)
    {
        if (stack.n > 0)
        {
            fragment_t fragment = pop_fragment_stack(&stack);
            *out = combine_fragment(&fragment, &fragment3);
            if (NULL == out->state)
            {
                free_fragment(&fragment, visited, visited1);
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
        fragment_t fragment = pop_fragment_stack(&stack);
        free_fragment(&fragment, visited, visited1);
    }
finally:
    return i;
}

static void traverse_states(state_set_t *state_set, state_t *state, int visited)
{
    if (state->visited != visited)
    {
        state->visited = visited;
        if (state->c != SPLIT)
        {
            state_set->states[state_set->count++] = state;
        }
        else
        {
            if (state->out != NULL)
            {
                traverse_states(state_set, state->out, visited);
            }
            if (state->out1 != NULL)
            {
                traverse_states(state_set, state->out1, visited);
            }
        }
    }
}

static state_set_t new_state_set(size_t size)
{
    state_set_t state_set = {NULL, 0, NULL};
    state_set.states = (state_t **)malloc(size * sizeof(state_t *));
    state_set.count = 0;
    return state_set;
}

static void free_state_set(state_set_t *state_set)
{
    if (state_set->states != NULL)
    {
        free(state_set->states);
        state_set->states = NULL;
        state_set->count = 0;
    }
}

static void next_states(state_set_t *out, state_set_t *states, int visited)
{
    for (size_t i = 0; i < states->count; i++)
    {
        if (states->states[i]->out != NULL)
        {
            traverse_states(out, states->states[i]->out, visited);
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

static int is_word_margin(const char *str, size_t i)
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

static int is_str_start(const char *str, size_t i)
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

static int is_str_end(const char *str, size_t i)
{
    if ('\0' == str[i] || '\n' == str[i])
    {
        return 1;
    }
    return 0;
}

static state_set_t *copy_new_state_set(const state_set_t *old)
{
    state_set_t *state_set = (state_set_t *)malloc(sizeof(state_set_t));
    if (NULL == state_set)
    {
        return NULL;
    }
    *state_set = new_state_set(old->count);
    if (NULL == state_set->states)
    {
        free(state_set);
        return NULL;
    }
    for (size_t i = 0; i < old->count; i++)
    {
        state_set->states[i] = old->states[i];
    }
    state_set->count = old->count;
    return state_set;
}

static void free_all_state_set(state_set_t *state_set)
{
    while (state_set != NULL)
    {
        state_set_t *next = state_set->next;
        free(state_set->states);
        free(state_set);
        state_set = next;
    }
}

static int check_pre_state(state_t *pre, const state_t *next, int visited)
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
                if (check_pre_state(pre->out, next, visited) == 0)
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
                if (check_pre_state(pre->out1, next, visited) == 0)
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
                if (check_pre_state(pre->out, next, visited) == 0)
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

static state_t *find_pre_state(const state_set_t *state_set, const state_t *state, int visited)
{
    state_t *ret = NULL;
    for (size_t i = 0; i < state_set->count; i++)
    {
        if (check_pre_state(state_set->states[i], state, visited) == 0)
        {
            ret = state_set->states[i];
            break;
        }
    }
    return ret;
}

static void init_regex_match(cregex_match_t *match)
{
    match->begin = (size_t)-1;
    match->len = 0;
}

static int match(cregex_t *regex, const char *text, size_t index, cregex_match_t *matchs, size_t n_match, int flag)
{   
    int ret = -1;
    state_set_t *root = NULL;
    for (size_t i = 0; i < n_match; i++)
    {
        init_regex_match(&matchs[i]);
    }
    regex->next.count = 0;
    regex->visited++;
    traverse_states(&regex->next, regex->root, regex->visited);
    state_t *match = NULL;
    size_t len = 0;
    size_t i = 0;
    for (;; i++)
    {
        regex->cmp.count = 0;
        for (size_t j = 0; j < regex->next.count; j++)
        {
            int c = regex->next.states[j]->c;
            switch (c)
            {
            case MATCH:
                match = regex->next.states[j];
                len = i;
                break;
            case WORD_MARGIN:
                if (is_word_margin(text, index + i) > 0)
                {
                    traverse_states(&regex->next, regex->next.states[j]->out, regex->visited);
                }
                break;
            case STR_START:
                if (is_str_start(text, index + i) > 0)
                {
                    traverse_states(&regex->next, regex->next.states[j]->out, regex->visited);
                }
                break;
            case STR_END:
                if (is_str_end(text, index + i) > 0)
                {
                    traverse_states(&regex->next, regex->next.states[j]->out, regex->visited);
                }
                break;
            default:
                if (c == (unsigned char)text[index + i])
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
            next_states(&regex->next, &regex->cmp, regex->visited);
            if ((flag & CREGEX_FLAG_NOSUB) != CREGEX_FLAG_NOSUB && n_match > 1)
            {
                state_set_t *state_set = copy_new_state_set(&regex->cmp);
                if (NULL == state_set)
                {
                    goto exception;
                }
                if (NULL == root)
                {
                    root = state_set;
                }
                else
                {
                    state_set->next = root;
                    root = state_set;
                }
            }
        }
        else if (match != NULL)
        {
            ret = (int)len;
            if (n_match > 0)
            {
                matchs[0].begin = index;
                matchs[0].len = len;
            }
            state_set_t *cur = root;
            for (; i > len && cur != NULL; i--)
            {
                cur = cur->next;
            }
            for (size_t i = len; cur != NULL; i--)
            {
                regex->visited++;
                match = find_pre_state(cur, match, regex->visited);
                for (int j = 0; j < MAX_GROUPS; j++)
                {
                    if (match->groups[j] >= 0)
                    {
                        int l = 0;
                        for (int k = j - 1; k >= 0; k--)
                        {
                            l += regex->groups[k];
                        }
                        l += match->groups[j];
                        if (l + 1 < n_match)
                        {
                            matchs[l + 1].begin = index + i - 1;
                            matchs[l + 1].len++;
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
        if ('\0' == text[index + i])
        {
            break;
        }
    }
exception:
finally:
    free_all_state_set(root);
    return ret;
}

cregex_t *cregex_compile(const char *pattern)
{
    cregex_t *regex = malloc(sizeof(cregex_t));
    if (NULL == regex)
    {
        goto exception;
    }
    regex->index = 0;
    size_t len = strlen(pattern);
    regex->cmp = new_state_set(256 * len);
    if (NULL == regex->cmp.states)
    {
        goto exception;
    }
    regex->next = new_state_set(256 * len);
    if (NULL == regex->next.states)
    {
        goto exception;
    }
    fragment_t fragment = init_fragment(NULL, NULL);
    int paren = -1;
    for (size_t i = 0; i < MAX_GROUPS; i++)
    {
        regex->groups[i] = 0;
    }
    regex->visited = 1;
    regex->visited1 = 1;
    int result = parse_pattern(&fragment, pattern, &paren, regex->groups, regex->visited, regex->visited1);
    if (result < 0 || paren != -1)
    {
        goto exception;
    }
    regex->match_state.c = MATCH;
    regex->match_state.out = NULL;
    regex->match_state.out1 = NULL;
    regex->match_state.copy = NULL;
    regex->match_state.visited = 0;
    regex->match_state.visited1 = 0;
    regex->match_state.del = NULL;
    fragment_t match = init_fragment(&regex->match_state, NULL);
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
    free_fragment(&fragment, regex->visited, regex->visited1);
    free_state_set(&regex->cmp);
    free_state_set(&regex->next);
    if (regex != NULL)
    {
        free(regex);
    }
    return NULL;
finally:
    return regex;
}

void cregex_free(cregex_t *regex)
{
    if (regex->root != NULL)
    {
        regex->visited++;
        regex->match_state.visited = regex->visited;
        regex->visited1++;
        free_state(regex->root, regex->visited, regex->visited1);
        free_state_set(&regex->next);
        free_state_set(&regex->cmp);
        free(regex);
    }
}

int cregex_match(cregex_t *regex, const char *text, cregex_match_t *matchs, size_t n_match, int flag)
{
    int len = match(regex, text, 0, matchs, n_match, flag);
    if (len >= 0 && '\0' == text[len])
    {
        return 0;
    }
    return -1;
}

int cregex_search(cregex_t *regex, const char *text, cregex_match_t *matchs, size_t n_match, int flag)
{
    while (text[regex->index] != '\0')
    {
        int len = match(regex, text, regex->index, matchs, n_match, flag);
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