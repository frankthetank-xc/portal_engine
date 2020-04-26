/**
 * Header file with common utilities for 3d math
 */

#ifndef __UTIL_H__
#define __UTIL_H__

// Global headers
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// Project headers
#include "common.h"

/* ***********************************
 * Public Definitions
 * ***********************************/

/* ***********************************
 * Public Macros
 * ***********************************/

/* ***********************************
 * Public Typedefs
 * ***********************************/

/* ***********************************
 * Public Function Prototypes
 * ***********************************/

int point_on_line(xy_t *p, xy_t *l0, xy_t *l1);
int point_on_line_raw(float px, float py, float l0x, float l0y, float l1x, float l1y);

int lines_intersect(xy_t *p0, xy_t *p1, xy_t *q0, xy_t *q2);

int lines_intersect_raw(float p0x, float p0y, float p1x, float p1y,
                        float q0x, float q0y, float q1x, float q1y );

void project_vector(float ax, float ay, float bx, float by, float *x, float *y);

#endif /*__UTIL_H__*/