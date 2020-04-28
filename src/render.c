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

#define MAX_PORTALS (32)
#define MAX_WALLS   (1024)

#define HFOV_DEFAULT (0.73f)
#define VFOV_DEFAULT (0.2f)

#define COLOR_CEILING   0x0F0F0F
#define COLOR_WALL      0xFF0000
#define COLOR_FLOOR     0x0F0F0F

// For use in calculating ceiling/floor
#define YAW(y,z) (y + z*player->yaw)

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
    float hfov_angle;
    float hfov;
    float vfov;
} render_settings_t;

/**
 * Structure to hold info about a wall as it gets
 * added to the render list.
 * These structs create a linked list in order
 * to enable fast removal of walls after they 
 * get constructed.
 */
typedef struct r_wall_struct r_wall_t;

struct r_wall_struct
{
    sector_t *sector;
    xy_t *v0, *v1;

    // Transformed coordinates (relative to player)
    xyz_t t0, t1;
    // Screen X positions (x0 is start, x1 is end)
    int x0, x1;

    int32_t neighbor;
    // Pointers to other objects in the queue for
    // fast removal
    r_wall_t *prev, *next;
};

/* ***********************************
 * Static variables
 * ***********************************/

/** Global window object */
static SDL_Window *_window;

/** Global renderer for easy access */
static SDL_Renderer *_renderer;

/** Global render settings */
static render_settings_t _rsettings;

/** Flag for if we should debug a frame */
static int debugging = 0;

// Variables for keeping track of 
static uint8_t  sectors_visited[MAX_SECTORS];
static r_wall_t wall_pool[MAX_WALLS];

/* ***********************************
 * Static function prototypes
 * ***********************************/

// Preprocessing step for rendering
r_wall_t *render_PreProcess(world_t *world);
// Check if wall is in front of another wall
int render_WallFront(r_wall_t *w1, r_wall_t *w2, player_t *player);
// Get the the next wall
r_wall_t *render_GetNextWall(r_wall_t **first, player_t *player);
// Draw a single wall
void render_DrawWall(r_wall_t *wall, world_t *world, int *ytop, int *ybottom);

/* ***********************************
 * Static function implementation
 * ***********************************/

/**
 * Rendering preprocessing step. Performs the following steps
 *  - Starting in the starting sector, add any *possibly*
 *      visible walls to a linked list of walls to be rendered
 *  - For every wall that links to another sector, add that neighbor
 *      to the rendering queue (unless it is already there!)
 */
r_wall_t *render_PreProcess(world_t *world)
{
    // Current sector
    sector_t *sect;
    static uint32_t wcount = 0;
    r_wall_t *last_wall = NULL, *wall = NULL, *first_wall = NULL;;
    xy_t ppos;
    float pcos, psin;
    // Portal flooding queue
    uint32_t rqueue[MAX_PORTALS], *rhead=rqueue, *rtail=rqueue;

    if(world == NULL) { return NULL; }

    // Get player position for later use
    ppos.x = world->player.pos.x;
    ppos.y = world->player.pos.y;

    // Get sin/cos of player angle for later use
    pcos = cosf(world->player.direction);
    psin = sinf(world->player.direction);

    // Initialize visited sectors list
    memset(sectors_visited, 0, MAX_SECTORS * sizeof(uint8_t));

    // Initialize render queue
    *rhead = world->player.sector;
    if(++rhead == rqueue + MAX_PORTALS) rhead = rqueue;
    
    do
    {
        // Get starting sector
        sect = &world->sectors[*rtail];
        // Mark sector as visited
        sectors_visited[*rtail] = 1;
        if(++rtail == rqueue + MAX_PORTALS) rtail = rqueue;

        // For each wall in the sector
        for(int i = 0; i < sect->num_vert; ++i)
        {
            // Get the two vertices
            xy_t *v0 = &world->vertices[sect->vertices[i+0]];
            xy_t *v1 = &world->vertices[sect->vertices[i+1]];
            xyz_t *t0, *t1;
            float xscale0, xscale1;
            int x0, x1;

            // Get pointer to the next active wall object
            wall = &wall_pool[wcount];

            // First, sanity check that the player can see this
            if(PointSide(ppos.x, ppos.y, v0->x, v0->y, v1->x, v1->y) < 0)
            {
                // Wall is facing the other way
                continue;
            }
            // Now check that the wall is within the player FOV
            // Generate points of the sector, rotated around player FOV
            t0 = &wall->t0; 
            t1 = &wall->t1;
            // Get rotated X coordinates
            t0->x = ((v0->x - ppos.x) * psin) - ((v0->y - ppos.y) * pcos);
            t1->x = ((v1->x - ppos.x) * psin) - ((v1->y - ppos.y) * pcos);
            // Get rotated Z coordinates
            t0->z = ((v0->x - ppos.x) * pcos) + ((v0->y - ppos.y) * psin);
            t1->z = ((v1->x - ppos.x) * pcos) + ((v1->y - ppos.y) * psin);

            // If wall is entirely behind player, it is not visible
            if(t0->z <= 0 && t1->z <= 0) continue;

            // The wall is partially behind the player, so we have to clip it against
            // the player's view frustrum
            if(t0->z <= 0 || t1->z <= 0)
            {
                float nearz = 1e-4f, farz = 5, nearside = 1e-6f, farside = 20.f;
                // Find an intersection between the wall and approximate edge of vision
                xy_t i1 = Intersect(t0->x, t0->z, t1->x, t1->z, -nearside, nearz, -farside, farz);
                xy_t i2 = Intersect(t0->x, t0->z, t1->x, t1->z,  nearside, nearz,  farside, farz);
                if(t0->z < nearz) { if(i1.y > 0) { t0->x = i1.x; t0->z = i1.y; } else { t0->x = i2.x; t0->z = i2.y; } }
                if(t1->z < nearz) { if(i1.y > 0) { t1->x = i1.x; t1->z = i1.y; } else { t1->x = i2.x; t1->z = i2.y; } }
            }

            // Transform t0 and t1 onto screen X values
            xscale0 = _rsettings.hfov / t0->z;  x0 = SCR_W/2 - (int)(t0->x * xscale0);
            xscale1 = _rsettings.hfov / t1->z;  x1 = SCR_W/2 - (int)(t1->x * xscale1);

            // Skip if the projected X values are out of range
            if(x0 >= x1 || x1 < 0 || x0 > (SCR_W - 1)) continue;

            // Wall is possibly visible. Add it to the queue, and queue up it's neighboring sector
            ++wcount;
            wcount = (wcount < MAX_WALLS) ? wcount : 0;

            if(last_wall) last_wall->next = wall;
            wall->prev = last_wall;
            wall->next = NULL;          
            wall->sector = sect;
            wall->v0 = v0;  wall->v1 = v1;
            wall->x0 = x0;  wall->x1 = x1;
            wall->neighbor = sect->neighbors[i];

            if(wall->neighbor > -1 && sectors_visited[wall->neighbor] == 0)
            {
                // Wall has a neighbor - queue up that sector
                *rhead = wall->neighbor;
                if(++rhead == rqueue + MAX_PORTALS) rhead = rqueue;
            }

            if(first_wall == NULL) first_wall = wall;

            // Mark this is the *last* wall visited
            last_wall = wall;
        }

    } while (rhead != rtail);
    
    return first_wall;
}

/**
 * Return 1 if w1 is in front of 2, or 0 otherwise.
 * Assumes the screen X coordinates of the lines don't intersect
 */
int render_WallFront(r_wall_t *w1, r_wall_t *w2, player_t *player)
{
    float t1, t2;
    float px, py;

    if(w1 == NULL || w2 == NULL || player == NULL) return 0;

    // If walls are in same sector + touching, they can't be blocking
    if(w1->sector == w2->sector)
    {
        if(w1->v0 == w2->v1 || w1->v1 == w2->v0) return 1;
    }

    // If wall Z values don't overlap, return closer one
    if(!Overlap(w1->t0.z, w1->t1.z, w2->t0.z, w2->t1.z))
    {
        return (w1->t0.z < w2->t0.z);
    }

    px = player->pos.x;
    py = player->pos.y;

    // Get cross products from first wall to second wall
    t1 = PointSide(w1->v0->x, w1->v0->y, w1->v1->x, w1->v1->y, w2->v0->x, w2->v0->y);
    t2 = PointSide(w1->v0->x, w1->v0->y, w1->v1->x, w1->v1->y, w2->v1->x, w2->v1->y);

    // 0 means parallel vectors
    if(FEQ(t1, 0))
    {
        // Wall2 Point 0 is on Wall1, so copy the valid xproduct over
        t1 = t2;
        if(FEQ(t1, 0))
        {
            // Parallel lines, return 1
            return 1;
        }
    }
    // Wall2 Point 1 is on Wall1, so copy the valid xproduct over
    if(FEQ(t2,0)) t2 = t1;

    // See if cross products have the same sign AKA same side of the wall
    if((t1 > 0 && t2 > 0) || (t1 < 0 && t2 < 0))
    {
        // Get cross product with origin camera
        t2 = PointSide(w1->v0->x, w1->v0->y, w1->v1->x, w1->v1->y, px, py);

        // True if player is on the other side of wall
        return ((t1 > 0 && t2 <= 0) || (t1 < 0 && t2 >= 0));
    }

    // Now do wall 2
    t1 = PointSide(w2->v0->x, w2->v0->y, w2->v1->x, w2->v1->y, w1->v0->x, w1->v0->y);
    t2 = PointSide(w2->v0->x, w2->v0->y, w2->v1->x, w2->v1->y, w1->v1->x, w1->v1->y);

    // 0 means parallel vectors
    if(FEQ(t1, 0))
    {
        // Wall2 Point 0 is on Wall1, so copy the valid xproduct over
        t1 = t2;
        if(FEQ(t1, 0))
        {
            // Parallel lines, return 1
            return 1;
        }
    }
    // Wall2 Point 1 is on Wall1, so copy the valid xproduct over
    if(FEQ(t2,0)) t2 = t1;

    // See if cross products have the same sign AKA same side of the wall
    if((t1 > 0 && t2 > 0) || (t1 < 0 && t2 < 0))
    {
        // Get cross product with origin camera
        t2 = PointSide(w2->v0->x, w2->v0->y, w2->v1->x, w2->v1->y, px, py);

        // False if player is on the other side of wall
        return ((t1 > 0 && t2 >= 0) || (t1 < 0 && t2 <= 0));
    }

    // Walls intersect
    return 1;
}


/**
 * Given a list of wall nodes, get the next one to render.
 * This will be a the first wall encounted that is not
 * interrupted by any other walls.
 * @note Will remove the returned node from the linked list
 */
r_wall_t *render_GetNextWall(r_wall_t **first, player_t *player)
{
    int cnt = 0;
    r_wall_t *next, *compare;

    if(first == NULL || *first == NULL || player == NULL) return NULL;

    next = *first;
    compare = next->next;

    while((compare != NULL) && (next != NULL))
    {
        // We are trying to compare a wall to itself
        if(next == compare) 
        { 
            compare = compare->next;
            continue;
        }
        // If x coords don't overlap, ignore this wall
        if(!Overlap(next->x0, next->x1, compare->x0, compare->x1))
        {
            compare = compare->next;
            continue;
        }

        // Is next in front of this wall?
        if(render_WallFront(next, compare, player))
        {
            // Increment compare and keep going
            compare = compare->next;
        }
        else
        {
            // Increment next and reset compare
            next = next->next;
            compare = *first;
        }
        
    }

    // IF we couldn't find any viable walls, just render front of wqueue
    if(next == NULL)
    {
        next = *first;
    }

    // Remove next from the queue
    //  Queue pool is a static circular buffer, so just need to get
    //  rid of any references 
    if(*first == next) *first = next->next;
    if(next->prev) next->prev->next = next->next;
    if(next->next) next->next->prev = next->prev;

    return next;
}

void render_DrawWall(r_wall_t *wall, world_t *world, int *ytop, int *ybottom)
{
    xyz_t *t0, *t1;
    int32_t neighbor;
    float yscale0, yscale1, yceil, yfloor, nyceil = 0, nyfloor = 0;
    sector_t *sect;
    player_t *player;
    int x0, x1;
    int y0a, y1a, y0b, y1b, ny0a, ny1a, ny0b, ny1b;
    int beginx, endx;

    if(wall == NULL || world == NULL || ytop == NULL || ybottom == NULL) return;

    t0 = &wall->t0; t1 = &wall->t1;
    x0 = wall->x0;  x1 = wall->x1;
    neighbor = wall->neighbor;
    sect = wall->sector;
    player = &world->player;
    // Set up y scaling
    yscale0 = _rsettings.vfov / t0->z;
    yscale1 = _rsettings.vfov / t1->z;

    // Get floor/ceiling heights
    yceil  = sect->ceil  - (player->pos.z + player->height);
    yfloor = sect->floor - (player->pos.z + player->height);

    // If we have a neighbor, get the floor/ceiling heights
    if(neighbor >= 0)
    {
        nyceil  = world->sectors[neighbor].ceil  - (player->pos.z + player->height);
        nyfloor = world->sectors[neighbor].floor - (player->pos.z + player->height);
    }

    // Project our ceiling & floor
    y0a = SCR_H/2 - (int)(YAW(yceil, t0->z) * yscale0), y0b = SCR_H/2 - (int)(YAW(yfloor, t0->z) * yscale0);
    y1a = SCR_H/2 - (int)(YAW(yceil, t1->z) * yscale1), y1b = SCR_H/2 - (int)(YAW(yfloor, t1->z) * yscale1);
    // Repeat for neighboring sector
    ny0a = SCR_H/2 - (int)(YAW(nyceil, t0->z) * yscale0), ny0b = SCR_H/2 - (int)(YAW(nyfloor, t0->z) * yscale0);
    ny1a = SCR_H/2 - (int)(YAW(nyceil, t1->z) * yscale1), ny1b = SCR_H/2 - (int)(YAW(nyfloor, t1->z) * yscale1);

    // Start wall rendering
    beginx = MAX(x0, 0);
    endx = MIN(x1, SCR_W);
    for(int x = beginx; x <= endx; ++x)
    {
        // Calc Z coordinate (only used for lighting)
        int z = ((x - x0) * (t1->z - t0->z) / (x1-x0) + t0->z) * 7;
        // Get Y for ceiling & floor, clamped to occlusion array
        int ya = (x-x0) * (y1a - y0a) / (x1-x0) + y0a, cya = CLAMP(ya, ytop[x], ybottom[x]);
        int yb = (x-x0) * (y1b - y0b) / (x1-x0) + y0b, cyb = CLAMP(yb, ytop[x], ybottom[x]);
        // Check if occlusion array shows we're done with this x coord
        if(ybottom[x] - ytop[x] < 2) continue;
        // Draw the ceiling
        render_draw_vline(x, ytop[x], cya-1, 0x222222);
        render_draw_point(x, ytop[x], 0x111111);
        render_draw_point(x, cya-1, 0x111111);
        // Draw floor
        render_draw_vline(x, cyb+1, ybottom[x], 0x0000AA);
        render_draw_point(x, cyb+1, 0x111111);
        render_draw_point(x, ybottom[x], 0x111111);
        if(neighbor >= 0)
        {
            // Draw *their* floor and ceiling
            int nya = (x-x0) * (ny1a-ny0a) / (x1-x0) + ny0a, cnya = CLAMP(nya, ytop[x], ybottom[x]);
            int nyb = (x-x0) * (ny1b-ny0b) / (x1-x0) + ny0b, cnyb = CLAMP(nyb, ytop[x], ybottom[x]);
            // If our ceiling is higher than theirs, render upper wall
            uint32_t r1 = 0x010101 * (255-MIN(z,255)), r2 = 0x040007 * (31-MIN(z/8,255/8));
            // Draw between our and their ceiling
            if(nyceil < yceil) 
            {
                render_draw_vline(x, cya, cnya-1, x==x0||x==x1 ? 0 :r1);
                render_draw_point(x, cya, 0);
                render_draw_point(x, cnya-1, 0);
            }
            // Shrink remaining window below these ceilings
            ytop[x] = CLAMP(MAX(cya, cnya), ytop[x], SCR_H-1);
            // If our floor is lower than theirs, render bottom wall.
            // Between their and our wall
            if(nyfloor > yfloor)
            {
                render_draw_vline(x, cnyb+1, cyb, x==x0||x==x1 ? 0 : r2);
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
            render_draw_vline(x, cya, cyb, x==x0||x==x1 ? 0 : r);
            render_draw_point(x, cya, 0);
            render_draw_point(x, cyb, 0);
            ytop[x] = ybottom[x];
        }
        
    }
}

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
    _rsettings.hfov_angle = HFOV_DEFAULT;
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
    // Initialize occlusion arrays
    int ytop[SCR_W]={0}, ybottom[SCR_W];
    uint32_t i;
    r_wall_t *wqueue = NULL;

    // Start with preprocessing
    wqueue = render_PreProcess(world);

    // Set up occlusion arrays
    for(int i = 0; i < SCR_W; ++i)
    {
        ytop[i] = 0;
        ybottom[i] = SCR_H - 1;
    }

    // Reset the whole screen
    render_reset_screen();

    while(wqueue != NULL)
    {
        // Get the next valid wall and draw it
        r_wall_t *next = render_GetNextWall(&wqueue, player);

        render_DrawWall(next, world, ytop, ybottom);
        if(debugging)
        {
            SDL_RenderPresent(_renderer);
            SDL_Delay(500);
        }
    }

    debugging = 0;
    render_draw_screen();
}


#if 0
// Old single pass renderer
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
#endif