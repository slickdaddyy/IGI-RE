#ifndef ERROR_H
#define ERROR_H

typedef void (*ErrorHandler)(const char *text);

void Error_Show(const char *fmt, ...);
void Warning_Show(const char *fmt, ...);
void Error_SetHandler(ErrorHandler fn);
void sub_4AF870(void *p);
void Warning_SetHandler(ErrorHandler fn);
void sub_4AF890(void *p);
void Error_SetExitHandler(ErrorHandler fn);
void sub_4AF8B0(int v);
void Error_ResetHandlers(void);

#endif
