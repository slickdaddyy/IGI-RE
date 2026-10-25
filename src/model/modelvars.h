#ifndef MODEL_MODELVARS_H
#define MODEL_MODELVARS_H

struct CollTask;

extern int gCollLineCalls;
extern int gCollLineCount;
extern int gCollSightCalls;
extern char gModelPath[];
extern char gModelDir[];
extern void (*gTaskEnterHandlers[])(struct CollTask *, int);
extern void (*gTaskLeaveHandlers[])(struct CollTask *, int);

#endif
