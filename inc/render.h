/**
 * File to wrap up raw SDL calls to simplify graphics
 */

#ifndef __RENDER_H__
#define __RENDER_H__

#include <stdio.h>
#include <stdint.h>
#include <SDL.h>

/* ***********************************
 * Public Definitions
 * ***********************************/

// Screen dimensions
#define SCR_H (480 * 2)
#define SCR_W (640 * 2)

/* ***********************************
 * Public Typedefs
 * ***********************************/

// Public structure to hold sprite info
typedef struct image_struct
{
    SDL_Texture *img;

    uint32_t w;
    uint32_t h;
} image_t;

/* ***********************************
 * Public Functions
 * ***********************************/

// SDL High Level Control
int8_t render_init(void);
int8_t render_close(void);

// Load Image
SDL_Texture *render_load_texture(const char *filename);
void render_free_image(image_t *image);

// High level screen controls
int8_t render_reset_screen(void);
int8_t render_draw_screen(void);
void render_delay(uint32_t ticks);

// Draw a rectangular image to the screen
int8_t render_draw_image(image_t *sprite, 
                    uint32_t x, uint32_t y);

// Draw a vertical line on the screen
int8_t render_draw_vline(uint32_t x, uint32_t y0, uint32_t y1, uint32_t color);
// Draw a point on the screen
int8_t render_draw_point(uint32_t x, uint32_t y, uint32_t color);

// Draw the game world
int8_t render_draw_world(void);

#endif /*__RENDER_H__*/