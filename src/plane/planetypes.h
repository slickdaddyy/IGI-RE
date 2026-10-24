#ifndef PLANE_PLANETYPES_H
#define PLANE_PLANETYPES_H

typedef struct RudderDef {
    int kind;
    float angle;
} RudderDef;

/* argument of the MagicObj definition parsers */
typedef struct DefParseArg {
    void *out;
    void *args;
    int index;
} DefParseArg;

#endif
