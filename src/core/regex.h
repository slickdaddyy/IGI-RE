#ifndef REGEX_H
#define REGEX_H

/* Pattern syntax: . [a-z] [~...] * ? + ( ) ^ $, {...} marks a group to
   capture, '/' (or '\' in classes) escapes the next character. */

enum {
    REGEX_END = 0x100,
    REGEX_STAR = 0x200,
    REGEX_GROUP_OPEN = 0x400,
    REGEX_GROUP_CLOSE = 0x800,
    REGEX_CLASS = 0x1000,
    REGEX_BEGIN = 0x2000,
    REGEX_EOL = 0x4000,
    REGEX_OPEN = 0x8000,
    REGEX_CLOSE = 0x10000,
    REGEX_REPEAT = 0x20000,
    REGEX_OPTIONAL = 0x40000,
};

/* op is a character, or one of the values above with flags */
typedef struct RegexState {
    int parent;
    int paren;
    int op;
    int next;
    int alt;
} RegexState;

/* a backtracking point, repeat counts how often pos can still step back */
typedef struct RegexFrame {
    int depth;
    int groups;
    int pos;
    int state;
    int repeat;
} RegexFrame;

typedef struct RegexGroup {
    int start;
    int len;
} RegexGroup;

int Regex_Compile(RegexState *prog, char *classes, const char *pattern);
void Regex_InitClasses(char *classes, int *count);
int Regex_Expand(char *out, const char *pattern);
int Regex_Run(int *end, int *groups, const char *str, RegexState *prog, const char *classes, int nocase);
int Regex_ClassHas(int c, const char *classes, int index);
void Regex_Push(RegexFrame *stack, int *top, int state, int pos, int groups, int depth);
int Regex_Pop(RegexFrame *stack, int *top, int *state, int *pos, int *groups, int *depth);
int Regex_Next(RegexState *prog, int state, int matched, int pos, int top);
int Regex_Search(int *start, int *end, int *groups, const char *str, RegexState *prog, const char *classes, int nocase);
int Regex_Find(int *start, int *end, int *groups, const char *pattern, const char *str, int nocase);
int Regex_Match(int *end, int *groups, const char *pattern, const char *str, int nocase);
void Wildcard_ToRegex(char *out, const char *wildcard);
int Wildcard_Find(int *start, int *end, int *groups, const char *wildcard, const char *str, int nocase);
int Regex_GetGroup(char *out, int size, const char *src, int group);
void Regex_Substitute(char *out, int size, const char *tmpl, const char *src);

#endif
