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
#define SCR_H (480 * 1)
#define SCR_W (640 * 1)

/* ***********************************
 * Public Typedefs
 * ***********************************/

typedef enum texture_name_enum
{
    TEXTURE_BRICK  = 0,
    TEXTURE_DIRT   = 1,
    TEXTURE_COBBLE = 2,
    NUM_TEXTURES   = 3
} texture_name_t;

// Public structure to hold sprite info
typedef struct image_struct
{
    SDL_Texture *img;
    uint32_t *pix;
    int pitch;

    uint32_t w;
    uint32_t h;
    // How many map units long is this texture (horiz/vert)
    float xscale, yscale;
} image_t;

/* ***********************************
 * Public Functions
 * ***********************************/

// SDL High Level Control
int8_t render_init(void);
int8_t render_close(void);

// Load Image
SDL_Texture *render_load_texture(const char *filename, uint32_t **pix, int *pitch);
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
int8_t render_vline_textured(uint32_t x, uint32_t y0, uint32_t y1, 
                             int ceil, int floor, 
                             image_t *texture, float height,
                             uint32_t idx, uint16_t z);
int8_t render_point_textured(uint32_t x, uint32_t y, uint32_t z,
                             image_t *texture, 
                             uint32_t tx, uint32_t ty);
// Draw a point on the screen
int8_t render_draw_point(uint32_t x, uint32_t y, uint32_t color);

void render_toggle_debug(void);
void render_set_fullscreen(int fs);

// Draw the game world
int8_t render_draw_world(void);

#endif /*__RENDER_H__*/