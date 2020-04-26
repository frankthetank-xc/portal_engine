// My header
#include "util.h"

// Global headers
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// Project headers
#include "common.h"

/* ***********************************
 * Private Definitions
 * ***********************************/

/* ***********************************
 * Private Typedefs
 * ***********************************/

/* ***********************************
 * Private variables
 * ***********************************/

/* ***********************************
 * Static function prototypes
 * ***********************************/

static void get_slope_intercept(float x0, float y0, float x1, float y1,
                                float *m, float *b);

/* ***********************************
 * Static function implementation
 * ***********************************/

/**
 * Convert a line in point format to slope intercept format, y = mx + b
 * @note assumes the line is not vertical
 */
static void get_slope_intercept(float x0, float y0, float x1, float y1,
                                float *m, float *b)
{
    if(m == NULL || b == NULL) return;

    // Check for flat line
    if(y0 == y1)
    {
        *m = 0.0;
        *b = y0;
        return;
    }
    // Check for vertical line - can't compute properly
    else if(x0 == x1)
    {
        *b = 0;
        *m = 9e9;
        return;
    }

    // Get the slope
    *m = (y1 - y0) / (x1 - x0);

    // Get the offset
    *b = y0 - ((*m) * x0);

    //printf("Slope Intercept: (%5.2f,%5.2f) (%5.2f,%5.2f) m = %5.2f b = %5.2f\n", x0,y0,x1,y1, *m, *b);

    return;
}

/* ***********************************
 * Public function implementation
 * ***********************************/

/**
 * Given a point p, check if it is on line L
 * @note This assumes that p is colinear to line L
 */
int point_on_line(xy_t *p, xy_t *l0, xy_t *l1)
{
    if(p == NULL || l0 == NULL || l1 == NULL) { return 0; }

    return point_on_line_raw(p->x, p->y, l0->x, l0->y, l1->x, l1->y);
}

/**
 * Given a point p, check if it is on line L
 * @note This assumes that p is colinear to line L
 */
int point_on_line_raw(float px, float py, float l0x, float l0y, float l1x, float l1y)
{
    if(px <= MAX(l0x, l1x) && px >= MIN(l0x, l1x)
      && py <= MAX(l0y, l1y) && py >= MIN(l0y, l1y))
    {
        return 1;
    }

    return 0;
}

/**
 * Check whether two lines, P and Q, intersect
 * @param[in] p0 First point of line P
 * @param[in] p1 Second point of line P
 * @param[in] q0 First point of line Q
 * @param[in] q1 Second point of line Q
 * @return 1 if lines intersect, 0 if they don't
 */
int lines_intersect(xy_t *p0, xy_t *p1, xy_t *q0, xy_t *q1)
{
    if((p0 == NULL) || (p1 == NULL) || (q0 == NULL) || (q1 == NULL))
    {
        return 0;
    }

    return lines_intersect_raw(p0->x, p0->y, p1->x, p1->y, 
                               q0->x, q0->y, q1->x, q1->y );
}

/**
 * Lower level lines_intersect that accepts raw coordinates.
 */
int lines_intersect_raw(float p0x, float p0y, float p1x, float p1y,
                        float q0x, float q0y, float q1x, float q1y )
{
    float t, u, denom;

    denom = ((p0x - p1x) * (q0y - q1y)) - ((p0y - p1y) * (q0x - q1x));

    if(FEQ(denom, 0.0))
    {
        return 0;
    }

    t = ((p0x - q0x) * (q0y - q1y)) - ((p0y - q0y) * (q0x - q1x));
    t /= denom;
    t = fabs(t);

    if(t < 0.0 || t > 1.0) return 0;

    u = ((p0x - p1x) * (p0y - q0y)) - ((p0y - p1y) * (p0x - q0x));
    u /= denom;
    u = fabs(u);

    if(u < 0.0 || u > 1.0) return 0;

    return 1;
}

/**
 * Projects a vector A onto another vector B.
 * 
 * @note Relatively inefficient, but called rarely enough
 * that there isn't much reason to optimize it
 * 
 * @param[in] ax x value of A
 * @param[in] ay y value of A
 * @param[in] bx x value of B
 * @param[in] by y value of B
 * @param[out] x Projected x value
 * @param[out] y Projected y value
 */
void project_vector(float ax, float ay, float bx, float by, float *x, float *y)
{
    float b_theta = atan2(by,bx);
    float ab_theta = atan2(ay,ax) - b_theta;
    float magnitude = sqrtf((ax*ax) + sqrt(ay*ay)) * cosf(ab_theta);

    if(x) *x = magnitude * cosf(b_theta);
    if(y) *y = magnitude * sinf(b_theta);
}