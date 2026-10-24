#ifndef SCRIPT_SCRIPTTYPES_H
#define SCRIPT_SCRIPTTYPES_H

/* A value returned by a symbol handler. Bytes 4 and 20 are never set. */
typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;
_Static_assert(sizeof(ScriptValue) == 0x18, "ScriptValue");

#endif
