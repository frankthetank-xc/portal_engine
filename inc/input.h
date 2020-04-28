/**
 * File to handle user inputs
 */

#ifndef __INPUT_H__
#define __INPUT_H__

#include "common.h"

/* ***********************************
 * Public Definitions
 * ***********************************/

/* ***********************************
 * Public Typedefs
 * ***********************************/

typedef struct keys_struct
{
    uint8_t w;
    uint8_t a;
    uint8_t s;
    uint8_t d;
    uint8_t e;
    uint8_t f;
    uint8_t c;
    uint8_t right;
    uint8_t left;
    uint8_t down;
    uint8_t up;
    uint8_t space;
    uint8_t q;
    uint8_t shift;
} keys_t;

/* ***********************************
 * Public Functions
 * ***********************************/

void input_init();
void input_update(void);
void input_set_mouselook(int enable);
void input_toggle_mouselook(void);
keys_t *input_get(void);
void mouse_get_input(int *x, int *y);

#endif /*__INPUT_H__*/