#include <string.h>

#include "hook.h"
#include "igi.h"
#include "core/regex.h"

ORIG(0x4AD73D, int, crt_toupper, (int c));

#define REGEX_MAX_FRAMES 0x200
#define REGEX_MAX_CLASSES 32
#define REGEX_MAX_GROUPS 10

extern char gRegexMatchClasses[];
extern RegexState gRegexFindProg[];
extern RegexGroup gRegexGroups[];
extern int gRegexMaxFrames;
extern RegexState gRegexMatchProg[];
extern int gRegexCounters[];
extern RegexFrame gRegexStack[];
extern char gRegexFindClasses[];
extern RegexGroup *gRegexResult;
extern int gRegexResultCount;

/* Bytes >= 0x80 inside [...] index the class table with a negative
   offset in the original, which overwrites its own locals. Those are
   masked here instead. */
#define CLASS_BYTE(c) ((unsigned char)(c))

int Regex_Compile(RegexState *prog, char *classes, const char *pattern)
{
    char pat[512];
    char set[256];
    int parents[256], opens[256];
    int *pp = parents, *os = opens;
    int depth = 0, nclasses = 0, inGroup = 0, nextParen = 0, paren = -1, lastOpen = 0;
    int len, i, s;
    RegexState *e;

    if (!Regex_Expand(pat, pattern))
        return 0;
    Regex_InitClasses(classes, &nclasses);
    len = strlen(pat);
    s = 0;
    e = prog;
    for (i = 0; i < len; i++, s++, e++) {
        switch (pat[i]) {
        case '^':
            e->op = REGEX_BEGIN;
            e->alt = -1;
            e->next = s + 1;
            break;
        case '$':
            e->op = REGEX_EOL;
            e->alt = -1;
            e->next = s + 1;
            break;
        case '.':
            e->op = REGEX_CLASS;
            e->alt = -1;
            e->next = s + 1;
            break;
        case '(':
            e->op = REGEX_OPEN;
            e->alt = -1;
            e->next = s + 1;
            e->parent = paren;
            e->paren = nextParen;
            *pp++ = paren;
            *os++ = s;
            paren = nextParen++;
            depth++;
            break;
        case ')':
            if (!depth)
                return 0;
            lastOpen = *--os;
            e->paren = paren;
            paren = *--pp;
            e->parent = paren;
            e->op = REGEX_CLOSE;
            e->alt = -1;
            e->next = s + 1;
            prog[lastOpen].alt = s + 1;
            depth--;
            break;
        case '*': {
            int prev;

            if (s == 0)
                return 0;
            prev = e[-1].op;
            e--;
            s--;
            if (prev >= 0x100 && !(prev & (REGEX_CLASS | REGEX_CLOSE)))
                return 0;
            if (prev & REGEX_CLOSE) {
                e->next = lastOpen;
                prog[lastOpen].op |= REGEX_REPEAT;
            } else {
                e->next = s;
            }
            e->op |= REGEX_STAR;
            e->alt = s + 1;
            break;
        }
        case '?':
            if (s == 0)
                return 0;
            e--;
            s--;
            if (e->op & REGEX_CLOSE)
                prog[lastOpen].op |= REGEX_REPEAT;
            e->next = s + 1;
            e->alt = s + 1;
            e->op |= REGEX_OPTIONAL;
            break;
        case '[': {
            int neg = 0, k;
            char c;

            memset(set, 0, sizeof(set));
            i++;
            if (pat[i] == '~') {
                i++;
                neg = 1;
            }
            for (c = pat[i]; c != ']'; c = pat[++i]) {
                if (i >= len)
                    return 0;
                if (c == '\\') {
                    switch (pat[++i]) {
                    case 0:
                        return 0;
                    case 'N':
                    case 'n':
                        set['\n'] = 1;
                        break;
                    case 'R':
                    case 'r':
                        set['\r'] = 1;
                        break;
                    case 'T':
                    case 't':
                        set['\t'] = 1;
                        break;
                    default:
                        set[CLASS_BYTE(pat[i])] = 1;
                        break;
                    }
                } else if (c == '/') {
                    set[CLASS_BYTE(pat[++i])] = 1;
                } else if (pat[i + 1] == '-') {
                    int hi = pat[i + 2], x;

                    i += 2;
                    if (c >= hi)
                        return 0;
                    for (x = c; x <= hi; x++)
                        set[CLASS_BYTE(x)] = 1;
                } else {
                    set[CLASS_BYTE(c)] = 1;
                }
            }
            if (neg)
                for (k = 0; k < 256; k++)
                    set[k] = !set[k];
            k = 0;
            if (nclasses > 0) {
                for (; k < nclasses; k++) {
                    const int *a = (const int *)set, *b = (const int *)(classes + k * 256);
                    int w;

                    for (w = 0; w < 64 && a[w] == b[w]; w++)
                        ;
                    if (w == 64)
                        break;
                }
                if (k >= REGEX_MAX_CLASSES)
                    return 0;
            }
            if (k == nclasses) {
                memcpy(classes + nclasses * 256, set, 256);
                nclasses++;
            }
            e->op = k | REGEX_CLASS;
            e->alt = -1;
            e->next = s + 1;
            break;
        }
        case '{':
            if (inGroup || depth)
                return 0;
            e->op = REGEX_GROUP_OPEN;
            e->alt = s + 1;
            e->next = s + 1;
            inGroup = 1;
            break;
        case '}':
            if (inGroup != 1 || depth)
                return 0;
            e->op = REGEX_GROUP_CLOSE;
            e->alt = s + 1;
            e->next = s + 1;
            inGroup = 0;
            break;
        case '/':
            i++;
            /* fall through */
        default:
            e->op = pat[i];
            e->alt = -1;
            e->next = s + 1;
            break;
        }
    }
    if (inGroup || depth)
        return 0;
    prog[s].op = REGEX_END;
    prog[s].alt = s;
    prog[s].next = s;
    return 1;
}
HOOK(0x4AF9D0, Regex_Compile);

/* class 0 is "." */
void Regex_InitClasses(char *classes, int *count)
{
    memset(classes, 1, 256);
    *count = 1;
}
HOOK(0x4AFF90, Regex_InitClasses);

/* Rewrites x+ as xx* for classes and (...) groups. A + after anything
   else stays a plain character. */
int Regex_Expand(char *out, const char *pattern)
{
    int parens[256];
    int depth = 0, inGroup = 0, noRepeat = 1, afterClass = 0, classStart = 0;
    int i = 0, o = 0, n, k;
    char c;

    if (!*pattern) {
        out[0] = 0;
        return 1;
    }
    do {
        c = pattern[i];
        switch (c) {
        case '/':
        case '\\':
            out[o++] = c;
            i++;
            out[o++] = pattern[i++];
            noRepeat = 0;
            afterClass = 0;
            break;
        case '[':
            classStart = o;
            while (c != ']') {
                if (!c)
                    return 0;
                if (c == '/' || c == '\\') {
                    out[o++] = c;
                    i++;
                }
                out[o++] = pattern[i++];
                c = pattern[i];
            }
            out[o++] = pattern[i++];
            afterClass = 1;
            noRepeat = 0;
            break;
        case '(':
            parens[depth++] = o;
            out[o++] = c;
            i++;
            noRepeat = 0;
            afterClass = 0;
            break;
        case ')':
            if (!depth)
                return 0;
            depth--;
            out[o++] = c;
            i++;
            noRepeat = 0;
            afterClass = 0;
            break;
        case '*':
        case '?':
            if (noRepeat)
                return 0;
            noRepeat = 1;
            out[o++] = c;
            i++;
            afterClass = 0;
            break;
        case '+':
            if (noRepeat)
                return 0;
            noRepeat = 1;
            if (afterClass) {
                n = o - classStart;
                for (k = 0; k < n; k++)
                    out[o + k] = out[classStart + k];
            } else if (pattern[i - 1] == ')') {
                n = o - parens[depth];
                for (k = 0; k < n; k++)
                    out[o + k] = out[parens[depth] + k];
            } else {
                out[o++] = c;
                i++;
                afterClass = 0;
                break;
            }
            o += n;
            afterClass = 0;
            out[o++] = '*';
            i++;
            break;
        case '{':
            if (depth || inGroup)
                return 0;
            inGroup = 1;
            out[o++] = c;
            i++;
            noRepeat = 0;
            afterClass = 0;
            break;
        case '}':
            if (!inGroup)
                return 0;
            inGroup = 0;
            out[o++] = c;
            i++;
            noRepeat = 0;
            afterClass = 0;
            break;
        default:
            out[o++] = c;
            i++;
            noRepeat = 0;
            afterClass = 0;
            break;
        }
    } while (pattern[i]);
    out[o] = 0;
    return 1;
}
HOOK(0x4AFFB0, Regex_Expand);

/* Matches prog at the start of str. *end gets the match length. */
int Regex_Run(int *end, int *groups, const char *str, RegexState *prog, const char *classes, int nocase)
{
    int len, pos = 0, top = 0, ngroups = 0, depth = 0, run = 0, open = 0, endPos = 0, popped;
    int state = 0;

    gRegexResult = NULL;
    gRegexResultCount = 0;
    len = strlen(str);
    if (groups)
        *groups = 0;
    if (end)
        *end = -1;
    for (;;) {
        RegexState *e;
        int op, matched;

        if (top >= REGEX_MAX_FRAMES)
            return 0;
        if (state == -1) {
            if (!Regex_Pop(gRegexStack, &top, &popped, &pos, &ngroups, &depth))
                return 0;
            state = Regex_Next(prog, popped, 0, pos, top);
            open = ngroups;
            run = 0;
        }
        e = &prog[state];
        op = e->op;
        if (op == REGEX_END) {
            if (pos >= 0)
                endPos = pos;
            if (groups)
                *groups = ngroups;
            gRegexResult = gRegexGroups;
            gRegexResultCount = ngroups;
            if (end)
                *end = endPos;
            return 1;
        }
        if (op == REGEX_GROUP_CLOSE) {
            state = Regex_Next(prog, state, 1, pos, top);
            if (--open < 0)
                return 0;
            gRegexGroups[ngroups - 1].len = pos - gRegexGroups[ngroups - 1].start + 1;
            continue;
        }
        if (op == REGEX_BEGIN) {
            state = Regex_Next(prog, state, pos == 0, pos, top);
            continue;
        }
        if (op == REGEX_EOL) {
            state = Regex_Next(prog, state, pos >= len, pos, top);
            continue;
        }
        if (op & REGEX_OPEN) {
            run = 0;
            gRegexCounters[e->paren] = 0;
            if (e->op & REGEX_REPEAT)
                Regex_Push(gRegexStack, &top, state, pos, ngroups, depth);
            state = Regex_Next(prog, state, 1, pos, top);
            depth++;
            continue;
        }
        if (op & REGEX_CLOSE) {
            if (e->parent >= 0)
                gRegexCounters[e->parent] += run;
            matched = 1;
            if (run == 0 && (run = gRegexCounters[e->paren]) == 0)
                matched = 0;
            state = Regex_Next(prog, state, matched, pos, top);
            depth--;
            continue;
        }
        if (op == REGEX_GROUP_OPEN) {
            gRegexGroups[ngroups].start = pos;
            gRegexGroups[ngroups].len = 0;
            state = Regex_Next(prog, state, 1, pos, top);
            open++;
            if (++ngroups >= REGEX_MAX_GROUPS)
                return 0;
            continue;
        }
        if (pos >= len) {
            state = Regex_Next(prog, state, 0, pos, top);
            continue;
        }
        if (op & REGEX_CLASS)
            matched = Regex_ClassHas(str[pos], classes, op & 0xFF);
        else if (nocase) {
            matched = crt_toupper(op & 0xFF);
            matched = matched == crt_toupper(str[pos]);
        }
        else
            matched = (op & 0xFF) == str[pos];
        if ((e->op & REGEX_STAR) && matched)
            Regex_Push(gRegexStack, &top, state, pos, ngroups, depth);
        if ((e->op & REGEX_OPTIONAL) && matched)
            Regex_Push(gRegexStack, &top, state, pos, ngroups, depth);
        state = Regex_Next(prog, state, matched, pos, top);
        if (matched) {
            run++;
            pos++;
        }
    }
}
HOOK(0x4B0270, Regex_Run);

int Regex_ClassHas(int c, const char *classes, int index)
{
    return classes[index * 256 + c];
}
HOOK(0x4B0670, Regex_ClassHas);

/* Pushing the same state one character further just bumps the repeat count. */
void Regex_Push(RegexFrame *stack, int *top, int state, int pos, int groups, int depth)
{
    int n = *top;
    RegexFrame *f;

    if (n) {
        f = &stack[n - 1];
        if (f->state == state && f->pos == pos - 1 && f->depth == depth) {
            f->repeat++;
            f->pos++;
            return;
        }
    }
    f = &stack[n++];
    f->repeat = 0;
    f->state = state;
    f->pos = pos;
    f->groups = groups;
    f->depth = depth;
    if (gRegexMaxFrames <= n)
        gRegexMaxFrames = n;
    *top = n;
}
HOOK(0x4B0690, Regex_Push);

int Regex_Pop(RegexFrame *stack, int *top, int *state, int *pos, int *groups, int *depth)
{
    int n = *top, s, ok;

    if (n > 0) {
        RegexFrame *f = &stack[--n];
        int p = f->pos, g = f->groups, d = f->depth;

        s = f->state;
        if (f->repeat) {
            f->repeat--;
            f->pos--;
            n++;
        }
        if (pos)
            *pos = p;
        if (groups)
            *groups = g;
        if (depth)
            *depth = d;
        ok = 1;
    } else {
        s = -1;
        ok = 0;
    }
    *top = n;
    if (state)
        *state = s;
    return ok;
}
HOOK(0x4B0720, Regex_Pop);

int Regex_Next(RegexState *prog, int state, int matched, int pos, int top)
{
    if (state < 0)
        return -1;
    return matched ? prog[state].next : prog[state].alt;
}
HOOK(0x4B07A0, Regex_Next);

int Regex_Search(int *start, int *end, int *groups, const char *str, RegexState *prog, const char *classes, int nocase)
{
    int len = strlen(str), i, n;

    for (i = 0; i < len; i++) {
        if (Regex_Run(&n, groups, str + i, prog, classes, nocase)) {
            if (start)
                *start = i;
            if (end)
                *end = n;
            return 1;
        }
    }
    return 0;
}
HOOK(0x4B07D0, Regex_Search);

int Regex_Find(int *start, int *end, int *groups, const char *pattern, const char *str, int nocase)
{
    if (!Regex_Compile(gRegexFindProg, gRegexFindClasses, pattern))
        return 0;
    return Regex_Search(start, end, groups, str, gRegexFindProg, gRegexFindClasses, nocase);
}
HOOK(0x4B0850, Regex_Find);

int Regex_Match(int *end, int *groups, const char *pattern, const char *str, int nocase)
{
    if (!Regex_Compile(gRegexMatchProg, gRegexMatchClasses, pattern))
        return 0;
    return Regex_Run(end, groups, str, gRegexMatchProg, gRegexMatchClasses, nocase);
}
HOOK(0x4B08A0, Regex_Match);

void Wildcard_ToRegex(char *out, const char *wildcard)
{
    int n = 1;

    out[0] = '^';
    if (*wildcard) {
        do {
            switch (*wildcard) {
            case '$':
            case '(':
            case ')':
            case '^':
                /* also skips the character after it */
                out[n++] = '/';
                out[n++] = *wildcard++;
                break;
            case '*':
                out[n++] = '.';
                out[n++] = '*';
                break;
            case '.':
                out[n++] = '/';
                out[n++] = '.';
                break;
            case '?':
                out[n++] = '.';
                break;
            case '/':
            case '\\':
                out[n++] = *wildcard++;
                out[n++] = *wildcard;
                break;
            default:
                out[n++] = *wildcard;
                break;
            }
            wildcard++;
        } while (*wildcard);
    }
    out[n] = '$';
    out[n + 1] = 0;
}
HOOK(0x4B08F0, Wildcard_ToRegex);

int Wildcard_Find(int *start, int *end, int *groups, const char *wildcard, const char *str, int nocase)
{
    char pattern[512];

    Wildcard_ToRegex(pattern, wildcard);
    return Regex_Find(start, end, groups, pattern, str, nocase);
}
HOOK(0x4B09C0, Wildcard_Find);

/* Copies {...} group number group of the last match in src. */
int Regex_GetGroup(char *out, int size, const char *src, int group)
{
    int i;

    if (gRegexResultCount <= group) {
        if (size)
            *out = 0;
        return 0;
    }
    if (!size)
        return 0;
    for (i = 0; i < size - 1 && i < gRegexResult[group].len - 1; i++)
        out[i] = src[gRegexResult[group].start + i];
    out[i] = 0;
    return 1;
}
HOOK(0x4B0A10, Regex_GetGroup);

/* Copies tmpl to out, replacing /0 to /9 with groups of the last match. */
void Regex_Substitute(char *out, int size, const char *tmpl, const char *src)
{
    int n = 0;
    char c;

    while (n < size - 1) {
        c = *tmpl;
        if (!c)
            break;
        if (c == '/' && (c = *++tmpl) >= '0' && c <= '9') {
            Regex_GetGroup(out + n, size - n, src, c - '0');
            n = strlen(out);
        } else {
            out[n++] = c;
        }
        tmpl++;
    }
    out[n] = 0;
}
HOOK(0x4B0A80, Regex_Substitute);
