/**
 * File to wrap up raw SDL calls to simplify graphics
 */

/* ***********************************
 * Includes
 * ***********************************/
// My header
#include "render.h"

// Global Headers
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <math.h>

// Project headers
#include "common.h"
#include "world.h"

/* ***********************************
 * Private Definitions
 * ***********************************/

#define MAX_PORTALS 32

#define HFOV_DEFAULT (0.73f)
#define VFOV_DEFAULT (0.2f)

#define COLOR_CEILING   0x0F0F0F
#define COLOR_WALL      0xFF0000
#define COLOR_FLOOR     0x0F0F0F

/* ***********************************
 * Private Typedefs
 * ***********************************/

/**
 * Structure to hold information about individual
 * items in the render queue
 */
typedef struct r_queue_struct
{
    int sectorN, sx1, sx2;
} r_queue_t;

typedef struct render_settings_struct
{
    float hfov;
    float vfov;
} render_settings_t;

/* ***********************************
 * Static variables
 * ***********************************/

/** Global window object */
static SDL_Window *_window;

/** Global renderer for easy access */
static SDL_Renderer *_renderer;

/** Global render settings */
static render_settings_t _rsettings;

static int debugging = 0;

/* ***********************************
 * Static function prototypes
 * ***********************************/

/* ***********************************
 * Static function implementation
 * ***********************************/

/* ***********************************
 * Public function implementation
 * ***********************************/

/**
 * Creates an SDL window
 * @return 0 on success
 */
int8_t render_init(void)
{
    SDL_Window *temp_window;
    int imgFlags;

    // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
    	printf("Init error %s\n", SDL_GetError());
    	return 1;
    }

    // Create window
    temp_window = SDL_CreateWindow("TestName", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    		SCR_W, SCR_H, SDL_WINDOW_SHOWN);
    if(temp_window == NULL)
    {
    	printf("No window %s\n", SDL_GetError());
    	return -1;
    }

    _window = temp_window;

    // Create renderer
    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Set render color to white
    SDL_SetRenderDrawColor(_renderer, 0xFF, 0xFF, 0xFF, 0xFF);

    // Initialize image loading
    imgFlags = IMG_INIT_PNG;
    if(!(IMG_Init(imgFlags) & imgFlags))
    {
        printf("SDL_image: %s\n", IMG_GetError());
        return -1;
    }

    // Set default render settings
    _rsettings.hfov = HFOV_DEFAULT * SCR_W;
    _rsettings.vfov = VFOV_DEFAULT * SCR_H;

    return 0;
}

/**
 * Closes SDL and the active window
 * @return 0 on success
 */
int8_t render_close(void)
{
    SDL_DestroyWindow(_window);

    _window = NULL;
    _renderer = NULL;

    SDL_Quit();

    return 0;
}

/**
 * Loads an image with name /filename and saves the surface to /image
 * @param[in] filename
 * @return 0 on success
 */
SDL_Texture *render_load_texture(const char *filename)
{
    SDL_Surface *surface;
    SDL_Texture *texture;

    // Switch based on filetype
    if(strstr(filename, ".bmp"))
    {
        surface = SDL_LoadBMP(filename);
    }
    else if(strstr(filename, ".png"))
    {
        surface = IMG_Load(filename);
    }
    else
    {
        printf("Filetype not supported for %s\n", filename);
        return NULL;
    }
    
    if(surface == NULL)
    {
    	printf("Could not load image %s!\nSDL Error: %s\n", filename, IMG_GetError());
    	return NULL;
    }

    // Optimize Surface
    texture = SDL_CreateTextureFromSurface(_renderer, surface);
    // Free temp surface
    SDL_FreeSurface(surface);

    // Return handle to texture
    return texture;
}

/**
 * Free an image_t structure
 */
void render_free_image(image_t *image)
{
    if(image == NULL)
    {
        return;
    }

    SDL_DestroyTexture(image->img);
    free(image);

    return;
}

/**
 * Reset the screen
 */
int8_t render_reset_screen(void)
{
    if(_renderer == NULL)
    {
        return -1;
    }
    // Set render color to white & clear screen with it
    SDL_SetRenderDrawColor(_renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(_renderer);

    return 0;
}

/**
 * Draw the current screen
 */
int8_t render_draw_screen(void)
{
    if(_renderer == NULL)
    {
        return -1;
    }

    SDL_RenderPresent(_renderer);

    return 0;
}

/**
 * Delay /ticks milliseconds
 * @param[in] ticks
 */
void render_delay(uint32_t ticks)
{
    SDL_Delay(ticks);
}

/** 
 * Draw a rectangular image to the screen
 * @param[in] sprite The sprite to draw
 * @param[in] x The x position of the image
 * @param[in] y The y position of the image 
 */
int8_t render_draw_image(image_t *image, 
                    uint32_t x, uint32_t y)
{
    SDL_Rect dest_rect;
    
    if(_renderer == NULL)
    {
        return -1;
    }

    dest_rect.h = image->h;
    dest_rect.w = image->w;
    dest_rect.x = x;
    dest_rect.y = y;

    SDL_RenderCopy(_renderer, image->img, NULL, &dest_rect);

    return 0;
}

/**
 * Draw a vertical line on the screen
 * @param[in] x The x position 
 * @param[in] y0 The start y
 * @param[in] y1 The end y
 * @param[in] color The color to draw
 */
int8_t render_draw_vline(uint32_t x, uint32_t y0, uint32_t y1, uint32_t color)
{
    uint8_t r, g, b;
    r = (color & 0xFF0000) >> 16;
    g = (color & 0x00FF00) >> 8;
    b = (color & 0x0000FF);

    SDL_SetRenderDrawColor(_renderer, r, g, b, 0xFF);

    SDL_RenderDrawLine(_renderer, x, y0, x, y1);
}

int8_t render_draw_point(uint32_t x, uint32_t y, uint32_t color)
{
    uint8_t r, g, b;
    r = (color & 0xFF0000) >> 16;
    g = (color & 0x00FF00) >> 8;
    b = (color & 0x0000FF);

    SDL_SetRenderDrawColor(_renderer, r, g, b, 0xFF);

    SDL_RenderDrawPoint(_renderer, x, y);
}

/**
 * 
 */
void toggle_debug(void)
{
    debugging = (debugging) ? 0 : 1;
}

/**
 * Draw the game world
 */
int8_t render_draw_world(void)
{
    // Get game world data
    world_t *world = world_get_world();
    player_t *player = &(world->player);
    int numSectors = world->numSectors;
    // Initialize render queue
    r_queue_t rqueue[MAX_PORTALS], *rhead=rqueue, *rtail=rqueue;
    // Initialize drawing bounds
    int ytop[SCR_W]={0}, ybottom[SCR_W], *rendered_sects;
    uint32_t i;

    rendered_sects = malloc(sizeof(int) * numSectors);

    for(i = 0; i < SCR_W; ++i) ybottom[i] = SCR_H - 1;
    for(i = 0; i < numSectors; ++i) rendered_sects[i] = 0;

    // First, reset the whole screen
    render_reset_screen();

    // Set HEAD to the player's position
    rhead->sectorN = player->sector; rhead->sx1 = 0; rhead->sx2 = SCR_W-1;
    if(++rhead == rqueue+MAX_PORTALS) rhead = rqueue;
    // Get player rotation
    float pcos = cosf(player->direction), psin = sinf(player->direction);

    // Begin render loop for world
    do
    {
        const r_queue_t now = *rtail;
        if(++rtail == rqueue + MAX_PORTALS) rtail = rqueue;

        if(rendered_sects[now.sectorN] & 0x21) continue; // Odd = still rendering. 20 = give up
        
        ++rendered_sects[now.sectorN];
        // Get a sector handle
        const sector_t *sect = &(world->sectors[now.sectorN]);

        // Render all of the walls facing the player
        for(i = 0; i < sect->num_vert; ++i)
        {
            // Vector IDs
            uint32_t vid1, vid2;
            // Vector coordinates
            xy_t v1, v2;
            // Translated vector coordinates
            xyz_t t1, t2;

            // Get the vertices, adjusted for player position
            vid1 = sect->vertices[i], vid2 = sect->vertices[i+1];
            v1.x = world->vertices[vid1].x - player->pos.x, v2.x = world->vertices[vid2].x - player->pos.x;
            v1.y = world->vertices[vid1].y - player->pos.y, v2.y = world->vertices[vid2].y - player->pos.y;
            // Rotate vertices around player view
            t1.x = v1.x * psin - v1.y * pcos;   t1.z = v1.x * pcos + v1.y * psin;
            t2.x = v2.x * psin - v2.y * pcos;   t2.z = v2.x * pcos + v2.y * psin;
            // Check if the wall is visible at all
            if(t1.z <= -1e-4f && t2.z <= -1e-4f) continue;

            // If wall is partially obscured, clip it against player's view
            if(t1.z <= 0 || t2.z <= 0)
            {
                float nearz = 1e-4f, farz = 5, nearside = 1e-6f, farside = 20.f;
                // Find intersection b/t wall and approximate edge of vision
                xy_t i1 = Intersect(t1.x, t1.z, t2.x, t2.z, -nearside, nearz, -farside, farz);
                xy_t i2 = Intersect(t1.x, t1.z, t2.x, t2.z, nearside, nearz, farside, farz);
                if(t1.z < nearz) { if(i1.y > 0) { t1.x = i1.x; t1.z = i1.y; } else { t1.x = i2.x; t1.z = i2.y; } }
                if(t2.z < nearz) { if(i1.y > 0) { t2.x = i1.x; t2.z = i1.y; } else { t2.x = i2.x; t2.z = i2.y; } }
            }

            // Run perspective transforms
            float xscale1 = _rsettings.hfov / t1.z, yscale1 = _rsettings.vfov / t1.z;
            float xscale2 = _rsettings.hfov / t2.z, yscale2 = _rsettings.vfov / t2.z;
            int x1 = SCR_W/2 - (int)(t1.x * xscale1);
            int x2 = SCR_W/2 - (int)(t2.x * xscale2);

            // Only render if visible
            if(x1 >= x2 || x2 < now.sx1 || x1 > now.sx2) continue;

            // Get floor/ceiling heights, relative to where player is viewing
            float yceil  = sect->ceil  - (player->pos.z + player->height);
            float yfloor = sect->floor - (player->pos.z + player->height);
            // Check the edge type. <0 means wall, other = boundary with another sector
            int neighbor = sect->neighbors[i];
            float nyceil=0, nyfloor=0;
            if(neighbor >= 0)
            {
                nyceil  = world->sectors[neighbor].ceil  - (player->pos.z + player->height);
                nyfloor = world->sectors[neighbor].floor - (player->pos.z + player->height);
            }
            
            // Project out ceiling & floor heights onto screen coordinates (Y coordinates)
            #define YAW(y,z) (y + z*player->yaw)
            int y1a = SCR_H/2 - (int)(YAW(yceil, t1.z) * yscale1), y1b = SCR_H/2 - (int)(YAW(yfloor, t1.z) * yscale1);
            int y2a = SCR_H/2 - (int)(YAW(yceil, t2.z) * yscale2), y2b = SCR_H/2 - (int)(YAW(yfloor, t2.z) * yscale2);
            // Repeat for neighboring sector
            int ny1a = SCR_H/2 - (int)(YAW(nyceil, t1.z) * yscale1), ny1b = SCR_H/2 - (int)(YAW(nyfloor, t1.z) * yscale1);
            int ny2a = SCR_H/2 - (int)(YAW(nyceil, t2.z) * yscale2), ny2b = SCR_H/2 - (int)(YAW(nyfloor, t2.z) * yscale2);

            // Start wall rendering loop
            int beginx = MAX(x1, now.sx1), endx = MIN(x2, now.sx2);
            for(int x = beginx; x <= endx; ++x)
            {
                // Calc Z coordinate (only used for lighting)
                int z = ((x - x1) * (t2.z - t1.z) / (x2-x1) + t1.z) * 7;
                // Get Y coords for ceiling & floor (clamped)
                int ya = (x-x1) * (y2a-y1a) / (x2-x1) + y1a, cya = CLAMP(ya, ytop[x], ybottom[x]);
                int yb = (x-x1) * (y2b-y1b) / (x2-x1) + y1b, cyb = CLAMP(yb, ytop[x], ybottom[x]);
                // Draw the ceiling
                render_draw_vline(x, ytop[x], cya-1, 0x222222);
                render_draw_point(x, ytop[x], 0x111111);
                render_draw_point(x, cya-1, 0x111111);
                // Draw floor
                render_draw_vline(x, cyb+1, ybottom[x], 0x0000AA);
                render_draw_point(x, cyb+1, 0x111111);
                render_draw_point(x, ybottom[x], 0x111111);

                // Is there a sector behind this edge?
                if(neighbor >= 0)
                {
                    // Draw *their* floor and ceiling
                    int nya = (x-x1) * (ny2a-ny1a) / (x2-x1) + ny1a, cnya = CLAMP(nya, ytop[x], ybottom[x]);
                    int nyb = (x-x1) * (ny2b-ny1b) / (x2-x1) + ny1b, cnyb = CLAMP(nyb, ytop[x], ybottom[x]);
                    // If our ceiling is higher than theirs, render upper wall
                    uint32_t r1 = 0x010101 * (255-MIN(z,255)), r2 = 0x040007 * (31-MIN(z/8,255/8));
                    // Draw between our and their ceiling
                    if(nyceil < yceil) 
                    {
                        render_draw_vline(x, cya, cnya-1, x==x1||x==x2 ? 0 :r1);
                        render_draw_point(x, cya, 0);
                        render_draw_point(x, cnya-1, 0);
                    }
                    // Shrink remaining window below these ceilings
                    ytop[x] = CLAMP(MAX(cya, cnya), ytop[x], SCR_H-1);
                    // If our floor is lower than theirs, render bottom wall.
                    // Between their and our wall
                    if(nyfloor > yfloor)
                    {
                        render_draw_vline(x, cnyb+1, cyb, x==x1||x==x2 ? 0 : r2);
                        render_draw_point(x, cnyb+1, 0);
                        render_draw_point(x, cyb, 0);
                    }
                    // Shrink the remaining window above these floors
                    ybottom[x] = CLAMP(MIN(cyb, cnyb), 0, ybottom[x]);
                }
                else
                {
                    // No neighbor, draw wall
                    uint32_t r = 0x010101 * (255- MIN(z, 255));
                    render_draw_vline(x, cya, cyb, x==x1||x==x2 ? 0 : r);
                    render_draw_point(x, cya, 0);
                    render_draw_point(x, cyb, 0);
                }
                
            } // End of wall loop
            // Schedule neighboring sector for rending within window
            if(neighbor >= 0 && endx >= beginx && (rhead+MAX_PORTALS+1-rtail)%MAX_PORTALS)
            {
                rhead->sectorN = neighbor; rhead->sx1 = beginx; rhead->sx2 = endx;
                if(++rhead == rqueue+MAX_PORTALS) rhead = rqueue;
            }

        } // End of sector loop
        if(debugging)
        {
            SDL_RenderPresent(_renderer);
            SDL_Delay(200);
        }
        ++rendered_sects[now.sectorN];
    } while (rhead != rtail);
    
    debugging = 0;
    // Draw final screen
    render_draw_screen();

    free(rendered_sects);
}