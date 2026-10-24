#ifndef ERROR_H
#define ERROR_H

typedef void (*ErrorHandler)(const char *text);

void Error_Show(const char *fmt, ...);
void Warning_Show(const char *fmt, ...);
void Error_SetHandler(ErrorHandler fn);
void Error_SetUnusedHandler(void *p);
void Warning_SetHandler(ErrorHandler fn);
void Error_SetMessageBoxHandler(void *p);
void Error_SetExitHandler(ErrorHandler fn);
void Error_SetUnusedFlag(int v);
void Error_ResetHandlers(void);

#endif
