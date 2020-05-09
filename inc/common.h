/**
 * Header file with common defines, macros, includes, etc
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* ***********************************
 * Public Definitions
 * ***********************************/

/* ***********************************
 * Public Macros
 * ***********************************/

/** min: Choose smaller of two scalars.*/
#define MIN(a,b)             (((a) < (b)) ? (a) : (b)) 

/** max: Choose greater of two scalars.*/
#define MAX(a,b)             (((a) > (b)) ? (a) : (b)) 

/** clamp: Clamp value into set range.*/
#define CLAMP(a, mi,ma)      MIN(MAX(a,mi),ma)         

/** vxs: Vector cross product.*/
#define vxs(x0,y0, x1,y1)    ((x0)*(y1) - (x1)*(y0))

// Overlap:  Determine whether the two number ranges overlap.
#define Overlap(a0,a1,b0,b1) (MIN(a0,a1) <= MAX(b0,b1) && MIN(b0,b1) <= MAX(a0,a1))

// IntersectBox: Determine whether two 2D-boxes intersect.
#define IntersectBox(x0,y0, x1,y1, x2,y2, x3,y3) (Overlap(x0,x1,x2,x3) && Overlap(y0,y1,y2,y3))

// PointSide: Determine which side of a line the point is on. Return value: <0, =0 or >0.
#define PointSide(px,py, x0,y0, x1,y1) vxs((x1)-(x0), (y1)-(y0), (px)-(x0), (py)-(y0))

// Intersect: Calculate the point of intersection between two lines.
#define Intersect(x1,y1, x2,y2, x3,y3, x4,y4) ((xy_t) { \
    vxs(vxs(x1,y1, x2,y2), (x1)-(x2), vxs(x3,y3, x4,y4), (x3)-(x4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)), \
    vxs(vxs(x1,y1, x2,y2), (y1)-(y2), vxs(x3,y3, x4,y4), (y3)-(y4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)) })

// Determine if two floats are equal
#define FEQ(a,b) (fabs(a - b) < 1e-3f)

#define PointOnLine(x0, y0, x1, y1, x) ((((x)-(x0)) * ((y1)-(y0)) / ((x1)-(x0))) + y0)

// Get magnitude of a line
#define LineMagnitude(x,y) (sqrt( ((x)*(x)) + ((y)*(y)) ) )

#define _xy(pt) pt.x, pt.y

#define PI (acos(-1))

/* ***********************************
 * Public Typedefs
 * ***********************************/

typedef struct xy_struct
{
    double x;
    double y;
} xy_t;

typedef struct xyz_struct
{
    double x;
    double y;
    double z;
} xyz_t;

#endif /*__COMMON_H__*/