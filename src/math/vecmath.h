#ifndef MATH_VECMATH_H
#define MATH_VECMATH_H

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24; /* sum of the inputs' values plus one on each multiply */
} Matrix;

typedef struct Quat {
    float x, y, z, w;
    int unk10;
} Quat;

typedef struct Random {
    unsigned int a, b, c;
} Random;

#define HALF_PI 0x1.921fb54442d1bp+0
#define PI      0x1.921fb54442d1bp+1
#define TWO_PI  0x1.921fb54442d1bp+2

void Vec3d_SetLength(Vec3d *v, double len);
void Vec3d_Normalize(Vec3d *v);
long double Vec3d_Length(const Vec3d *v);
long double Vec3d_LengthSq(const Vec3d *v);
void Vec3d_Cross(Vec3d *out, const Vec3d *a, const Vec3d *b);
long double Vec3d_Angle(const Vec3d *a, const Vec3d *b);
long double Vec3d_Distance(const Vec3d *a, const Vec3d *b);
long double Vec3d_DistanceSq(const Vec3d *a, const Vec3d *b);
int Vec3d_IsZero(const Vec3d *v);
int Vec3d_Equal(const Vec3d *a, const Vec3d *b);

void Matrix_RotateEuler(Matrix *out, const Matrix *m, float ax, float ay, float az);
void Matrix_RotateX(Matrix *out, const Matrix *m, float a);
void Matrix_RotateY(Matrix *out, const Matrix *m, float a);
void Matrix_RotateZ(Matrix *out, const Matrix *m, float a);
void Matrix_SetEuler(Matrix *m, float ax, float ay, float az);
void Matrix_SetRotX(Matrix *m, float a);
void Matrix_SetRotY(Matrix *m, float a);
void Matrix_SetRotZ(Matrix *m, float a);
void Matrix_SetScale(Matrix *m, float sx, float sy, float sz);
void Matrix_GetEuler(const Matrix *m, float *ax, float *ay, float *az);
void Matrix_SetAxisAngle(Matrix *m, const Vec3f *axis, float angle);
void Matrix_SetDirection(Matrix *m, const Vec3f *dir);
void Matrix_Invert(Matrix *out, const Matrix *m);

void Quat_SlerpPrepare(char *flip, float *angle, float *inv_sin, const Quat *a, const Quat *b);
long double Quat_Dot(const Quat *a, const Quat *b);
void Quat_SetEuler(Quat *q, float ax, float ay, float az);
void Quat_SetAxisAngle(Quat *q, const Vec3f *axis, float angle);

void Math_InitTables(void);

void Random_Init(Random *r);
unsigned int Random_Next(Random *r);
long double Random_Float(Random *r);
unsigned int Random_Int(Random *r, unsigned int n);
void Random_Seed(Random *r, unsigned int seed);

#endif
