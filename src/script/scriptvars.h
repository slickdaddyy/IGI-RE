#ifndef SCRIPT_SCRIPTVARS_H
#define SCRIPT_SCRIPTVARS_H

extern void *gScriptErrorContext;
extern void *gAsmErrorContext;
extern void (*gAsmErrorHandler)(const char *msg, void *data);
extern int gScriptWarningLevel;
extern int gScriptError;

#endif
