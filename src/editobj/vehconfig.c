/* The common part of the vehicle physics config, read from script arguments. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "editobj/editobj.h"

/* per type data of a vehicle physics type */
typedef struct VehicleConfig {
    float unk0;         /* start speed scale */
    struct {
        float rate;
        float smooth;
    } axis[3];
    float thrust;
    float accel;
    float accel_slow;
    int part_count;
    float parts[8][7];
    float unk10C[3];
} VehicleConfig;

_Static_assert(sizeof(VehicleConfig) == 0x118, "VehicleConfig");

ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4EC4D0, void, AeroPart_Init, (float *p, float area, float x, float y, float z, float nx, float ny, float nz));

static const float kUnits = 4096.0f;
static const float kHalf = 0.5f;
static const float kDegToRad = 0x1.1df46ap-6f;

void Vehicle_ParseConfig(VehicleConfig *cfg, int *next, void *args)
{
    float *f = &cfg->unk0;
    int i;

    for (i = 0; i < 10; i++)
        f[i] = Script_GetArgReal(args, (*next)++);
    cfg->part_count = Script_GetArgInt(args, (*next)++);
    for (i = 0; i < cfg->part_count; i++) {
        float nz = Script_GetArgReal(args, *next + 6);
        float ny = Script_GetArgReal(args, *next + 5);
        float nx = Script_GetArgReal(args, *next + 4);
        float z = Script_GetArgReal(args, *next + 3) * kUnits;
        float y = Script_GetArgReal(args, *next + 2) * kUnits;
        float x = Script_GetArgReal(args, *next + 1) * kUnits;
        float area = Script_GetArgReal(args, *next);

        AeroPart_Init(cfg->parts[i], area, x, y, z, nx, ny, nz);
        *next += 7;
    }
    cfg->unk10C[0] = Script_GetArgReal(args, (*next)++) * kUnits;
    cfg->unk10C[1] = Script_GetArgReal(args, (*next)++) * kHalf * kDegToRad;
    cfg->unk10C[2] = Script_GetArgReal(args, (*next)++) * kHalf * kDegToRad;
}
HOOK(0x5031F0, Vehicle_ParseConfig);
