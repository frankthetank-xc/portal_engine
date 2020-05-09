/**
 * File to wrap up raw SDL calls to simplify graphics
 * @note World files are defined with all sectors going counterclockwise,
 * and all inner walls in sectors going clockwise. When rendered, v0 and
 * v1 of a sector get flipped to make sure the orientation is correct.
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

#define DIST_SHADE_MULT (1)

#define SKYBOX_NAME "resource/citybg.bmp"
#define SKYBOX_W (_skybox.w * HFOV_DEFAULT / (PI))
#define SKYBOX_H (_skybox.h * VFOV_DEFAULT * 2)

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
    double hfov_angle;
    double hfov;
    double vfov;
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
    // Texture start & end
    int32_t texture, lo_texture, hi_texture;
    int u0, u1;

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

static const char *_texture_names[NUM_TEXTURES] = 
{
    "resource/brick.bmp",
    "resource/dirt.bmp",
    "resource/cobble.bmp",
    "resource/crosshatch.bmp",
    "resource/drywall.bmp",
    "resource/moss.bmp",
    "resource/rock.bmp",
    "resource/rustysheet.bmp",
    "resource/smoothstone.bmp"
};

static image_t _textures[NUM_TEXTURES] =
{
    {NULL,NULL,0,0,0, 5, 20},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15},
    {NULL,NULL,0,0,0, 5, 15}
};

static image_t _skybox;

// For drawing anything with direct pixels
static SDL_Texture *_screen_buffer;
static SDL_PixelFormat *_fmt;
static uint32_t *_scr_pix;
static int _scr_pitch;

// Variables for keeping track of 
static uint8_t  sectors_visited[MAX_SECTORS];
static r_wall_t wall_pool[MAX_WALLS];

static int *ytop = NULL;
static int *ybottom = NULL;

/* ***********************************
 * Static function prototypes
 * ***********************************/

// Draw the skybox onto the static pixel buffer
void render_draw_skybox(player_t *player);

// Preprocessing step for rendering
r_wall_t *render_PreProcess(world_t *world);

// Check if wall is in front of another wall
int render_WallFront(r_wall_t *w1, r_wall_t *w2, player_t *player);

// Get the the next wall
r_wall_t *render_GetNextWall(r_wall_t **first, player_t *player);

// Draw a single wall
void render_DrawWall(r_wall_t *wall, world_t *world, int *ytop, int *ybottom);

// Go from screen coords to map coords
void render_screen_to_world(int sX, int sY, double mapY, double pcos, double psin, player_t *player, double *mapX, double *mapZ);

/* ***********************************
 * Static function implementation
 * ***********************************/

/**
 * Draws the skybox onto the static pixel buffer
 */
void render_draw_skybox(player_t *player)
{
    // Get starting x
    int x0 = _skybox.w - ((player->direction * _skybox.w) / (2 * PI));
    int y0 = (_skybox.h / 2) + ((player->yaw / MAX_YAW) *  (_skybox.h / 2));
    int x1 = (x0 + SKYBOX_W);
    int y1 = y0 + SKYBOX_H;

    for(int x = 0; x < SCR_W; ++x)
    {
        for(int y = 0; y < SCR_H; ++y)
        {
            int ix = PointOnLine(0, x0, SCR_W, x1, x) % _skybox.w;
            int iy = PointOnLine(0, y0, SCR_H, y1, y);
            iy = MAX(iy, 0);
            iy = MIN(iy, _skybox.h - 1);
            _scr_pix[(y * _scr_pitch / sizeof(uint32_t)) + x] = 
                _skybox.pix[(iy * _skybox.pitch / sizeof(uint32_t)) + ix];
        }
    }
}

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
    double pcos, psin;
    // Portal flooding queue
    uint32_t rqueue[MAX_PORTALS], *rhead=rqueue, *rtail=rqueue;

    if(world == NULL) { return NULL; }

    // Get player position for later use
    ppos.x = world->player.pos.x;
    ppos.y = world->player.pos.y;

    // Get sin/cos of player angle for later use
    pcos = cos(world->player.direction);
    psin = sin(world->player.direction);

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
        for(int i = 0; i < sect->num_walls; ++i)
        {
            // Get the two vertices
            xy_t *v0 = &world->vertices[sect->walls[i].v1];
            xy_t *v1 = &world->vertices[sect->walls[i].v0];
            xyz_t *t0, *t1;
            double xscale0, xscale1;
            // TODO TODO configurable textures
            image_t *texture = NULL;
            double texture_scale = 1;
            int tw = 100;
            if(sect->walls[i].texture_mid < NUM_TEXTURES)
            {
                texture = &_textures[sect->walls[i].texture_mid];
                
            }
            else if(sect->walls[i].texture_low < NUM_TEXTURES)
            {
                texture = &_textures[sect->walls[i].texture_low];
            }
            else if(sect->walls[i].texture_high < NUM_TEXTURES)
            {
                texture = &_textures[sect->walls[i].texture_high];
            }
            if(texture != NULL)
            {
                texture_scale = texture->xscale;
                tw = texture->w;
            }
            
            int x0, x1, u0, u1;

            // Get pointer to the next active wall object
            wall = &wall_pool[wcount];

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

            // Set up texture mapping variables
            u0 = 0;
            u1 = LineMagnitude(((v1->x - v0->x) / texture_scale), ((v1->y - v0->y) / texture_scale)) * tw;
            // The wall is partially behind the player, so we have to clip it against
            // the player's view frustrum
            if(t0->z <= 0 || t1->z <= 0)
            {
                double nearz = 1e-5f, farz = 5, nearside = 0e-5f, farside = 50.f;
                xy_t org0 = {t0->x, t0->z}, org1 = {t1->x, t1->z};
                // Find an intersection between the wall and approximate edge of vision
                xy_t i1 = Intersect(t0->x, t0->z, t1->x, t1->z, -nearside, nearz, -farside, farz);
                xy_t i2 = Intersect(t0->x, t0->z, t1->x, t1->z,  nearside, nearz,  farside, farz);
                if(t0->z < 0) { if(i1.y > 0) { t0->x = i1.x; t0->z = i1.y; } else { t0->x = i2.x; t0->z = i2.y; } }
                if(t1->z < 0) { if(i2.y > 0) { t1->x = i2.x; t1->z = i2.y; } else { t1->x = i1.x; t1->z = i1.y; } }
                // Adjust texture mapping values
                if(fabs(t1->x - t0->x) > fabs(t1->z - t0->z))
                {
                    u0 = PointOnLine(org0.x, 0, org1.x, u1, t0->x);
                    u1 = PointOnLine(org0.x, 0, org1.x, u1, t1->x);
                }
                else
                {
                    u0 = PointOnLine(org0.y, 0, org1.y, u1, t0->z);
                    u1 = PointOnLine(org0.y, 0, org1.y, u1, t1->z);
                }
            }

            // Transform t0 and t1 onto screen X values
            xscale0 = (_rsettings.hfov_angle * SCR_H) / t0->z;  x0 = SCR_W/2 + (int)(t0->x * xscale0);
            xscale1 = (_rsettings.hfov_angle * SCR_H) / t1->z;  x1 = SCR_W/2 + (int)(t1->x * xscale1);

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
            wall->neighbor = sect->walls[i].neighbor;
            wall->u0 = u0;  wall->u1 = u1;
            wall->texture = sect->walls[i].texture_mid;
            wall->lo_texture = sect->walls[i].texture_low;
            wall->hi_texture = sect->walls[i].texture_high;

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
    double t1, t2;
    double px, py;

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
    double yscale0, yscale1, yceil, yfloor, nyceil = 0, nyfloor = 0;
    sector_t *sect, *nbr = NULL;
    player_t *player;
    int x0, x1;
    int y0a, y1a, y0b, y1b, ny0a, ny1a, ny0b, ny1b;
    int beginx, endx;
    int u0, u1;
    double pcos, psin;

    if(wall == NULL || world == NULL || ytop == NULL || ybottom == NULL) return;

    t0 = &wall->t0; t1 = &wall->t1;
    x0 = wall->x0;  x1 = wall->x1;
    neighbor = wall->neighbor;
    sect = wall->sector;
    player = &world->player;
    u0 = wall->u0, u1 = wall->u1;
    // Set up y scaling
    yscale0 = _rsettings.vfov / t0->z;
    yscale1 = _rsettings.vfov / t1->z;

    // Get floor/ceiling heights
    yceil  = sect->ceil  - (player->pos.z + player->height);
    yfloor = sect->floor - (player->pos.z + player->height);

    // Get cos/sin
    pcos = cos(player->direction);
    psin = sin(player->direction);

    // If we have a neighbor, get the floor/ceiling heights
    if(neighbor >= 0)
    {
        nbr = &world->sectors[neighbor];
        nyceil  = nbr->ceil  - (player->pos.z + player->height);
        nyfloor = nbr->floor - (player->pos.z + player->height);
    }

    // Project our ceiling & floor
    y0a = SCR_H/2 + (int)(-YAW(yceil, t0->z) * yscale0), y0b = SCR_H/2 + (int)(-YAW(yfloor, t0->z) * yscale0);
    y1a = SCR_H/2 + (int)(-YAW(yceil, t1->z) * yscale1), y1b = SCR_H/2 + (int)(-YAW(yfloor, t1->z) * yscale1);
    // Repeat for neighboring sector
    ny0a = SCR_H/2 + (int)(-YAW(nyceil, t0->z) * yscale0), ny0b = SCR_H/2 + (int)(-YAW(nyfloor, t0->z) * yscale0);
    ny1a = SCR_H/2 + (int)(-YAW(nyceil, t1->z) * yscale1), ny1b = SCR_H/2 + (int)(-YAW(nyfloor, t1->z) * yscale1);

    // Start wall rendering
    beginx = MAX(x0, 0);
    endx = MIN(x1, SCR_W);
    for(int x = beginx; x <= endx; ++x)
    {
        // Calculate persective-corrected pixel index in wall texture
        //    FROM wikipedia article on affine texture mapping
        int texture_idx = (u0*((x1-x)*t1->z) + u1*((x-x0)*t0->z)) / ((x1-x)*t1->z + (x-x0)*t0->z);
        // Calc Z coordinate (only used for lighting)
        int z = PointOnLine(x0, t0->z, x1, t1->z, x) * DIST_SHADE_MULT;
        if(z < 0) continue;
        // Get Y for ceiling & floor, clamped to occlusion array
        int ya = PointOnLine(x0, y0a, x1, y1a, x), cya = CLAMP(ya, ytop[x], ybottom[x]);
        int yb = PointOnLine(x0, y0b, x1, y1b, x), cyb = CLAMP(yb, ytop[x], ybottom[x]);
        // Check if occlusion array shows we're done with this x coord
        if(ybottom[x] - ytop[x] < 1) continue;

        // Loop to draw ceiling and floor
        for(int y = ytop[x]; y <= ybottom[x] && y < SCR_H; ++y)
        {
            // If we finish ceiling, switch to floor
            if(y >= cya && y <= cyb) { y = cyb; continue; }
            double height = (y < cya) ? yceil : yfloor;
            double mapx, mapy;
            int xi, yi;
            int16_t ftx = (y < cya) ? sect->texture_ceil : sect->texture_floor;
            if(!IS_TEXTURE(ftx)) continue;

            image_t *ftexture = (y < cya) ? &_textures[sect->texture_ceil] : &_textures[sect->texture_floor];
            render_screen_to_world(x, y, height, pcos, psin, player, &mapx, &mapy);
            // yscale is for vertical scaling only,
            // so use xscale even for the y. 
            xi = mapx * ftexture->w / ftexture->xscale;
            yi = mapy * ftexture->h / ftexture->xscale;
            if(xi < 0) xi += ((-xi) / ftexture->w) * ftexture->w;
            if(yi < 0) yi += ((-yi) / ftexture->h) * ftexture->h;
            // Get depth for this floor/ceil piece
            double tz = ((mapx - player->pos.x) * pcos) + ((mapy - player->pos.y) * psin);
            // Draw the point
            render_point_textured(x, y, (int)MAX(tz,0) * DIST_SHADE_MULT, ftexture, xi, yi, wall->sector->brightness);
        }

        if(neighbor >= 0)
        {
            // Draw *their* floor and ceiling
            int nya = PointOnLine(x0, ny0a, x1, ny1a, x), cnya = CLAMP(nya, ytop[x], ybottom[x]);
            int nyb = PointOnLine(x0, ny0b, x1, ny1b, x), cnyb = CLAMP(nyb, ytop[x], ybottom[x]);
            // If our ceiling is higher than theirs, render upper wall
            // Draw between our and their ceiling
            if(nyceil < yceil && IS_TEXTURE(wall->hi_texture) && nya > 0) 
            {
                render_vline_textured_bitwise(x, cya, cnya-1, ya, nya, &_textures[wall->hi_texture],
                                      sect->ceil - nbr->ceil, texture_idx, z, wall->sector->brightness);
            }
            // Shrink remaining window below these ceilings
            ytop[x] = CLAMP(MAX(cya, cnya), ytop[x], SCR_H-1);
            // If our floor is lower than theirs, render bottom wall.
            // Between their and our wall
            if(nyfloor > yfloor && IS_TEXTURE(wall->lo_texture) && nyb < (SCR_H - 1))
            {
                render_vline_textured_bitwise(x, cnyb+1, cyb, nyb, yb, &_textures[wall->lo_texture], 
                                      nbr->floor - sect->floor, texture_idx, z, wall->sector->brightness);
            }
            // Shrink the remaining window above these floors
            ybottom[x] = CLAMP(MIN(cyb, cnyb), 0, ybottom[x]);
        }
        else
        {
            // No neighbor, draw wall
            if(IS_TEXTURE(wall->texture))
                render_vline_textured_bitwise(x, cya, cyb, ya, yb, &_textures[wall->texture], sect->ceil - sect->floor, texture_idx, z, wall->sector->brightness);
            ytop[x] = ybottom[x];
        }
    }
}

/**
 * Convert a screen coordinate to a world one
 */
void inline render_screen_to_world(int sX, int sY, double mapY, double pcos, double psin, player_t *player, double *mapX, double *mapZ)
{
    double tz, tx;
    if(!mapX || !mapY) return;
    *mapZ = mapY * _rsettings.vfov / (((SCR_H / 2) - sY) - (player->yaw * _rsettings.vfov));
    *mapX = (*mapZ) * (sX - (SCR_W / 2)) / (_rsettings.hfov_angle * SCR_H);

    tx = (*mapZ) * pcos + (*mapX) * psin;
    tz = (*mapZ) * psin - (*mapX) * pcos;
    *mapX = tx + player->pos.x;
    *mapZ = tz + player->pos.y;
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
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
    {
    	printf("Init error %s\n", SDL_GetError());
    	return 1;
    }

    // Create window
    temp_window = SDL_CreateWindow("TestName", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    		SCR_W, SCR_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if(temp_window == NULL)
    {
    	printf("No window %s\n", SDL_GetError());
    	return -1;
    }

    _window = temp_window;

    // Create renderer
    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(_renderer == NULL)
    {
        printf("Can't make renderer: %s\n", SDL_GetError());
    }

    SDL_RenderSetLogicalSize(_renderer, SCR_W, SCR_H);

    // Set render color to white
    SDL_SetRenderDrawColor(_renderer, 0xFF, 0xFF, 0xFF, 0xFF);

    _screen_buffer = SDL_CreateTexture(_renderer, SDL_GetWindowPixelFormat(_window),
                                          SDL_TEXTUREACCESS_STREAMING,
                                          SCR_W, SCR_H);
    if(_screen_buffer == NULL)
    {
        printf("Can't make screen buffer: %s\n", SDL_GetError());
    }
    SDL_SetTextureBlendMode(_screen_buffer, SDL_BLENDMODE_ADD);
    _fmt = SDL_AllocFormat(SDL_GetWindowPixelFormat(_window));
    _scr_pix = NULL;
    _scr_pitch = 0;

    // Initialize image loading
    imgFlags = IMG_INIT_PNG;
    if(!(IMG_Init(imgFlags) & imgFlags))
    {
        printf("SDL_image: %s\n", IMG_GetError());
        return -1;
    }

    // Load all textures
    for(int i = 0; i < NUM_TEXTURES; ++i)
    {
        _textures[i].img = render_load_texture(_texture_names[i], &_textures[i].pix, &_textures[i].pitch);
        if(_textures[i].img == NULL)
        {
            // ERROR
            printf("ERROR: Can't load texture %s\n", _texture_names[i]);
            return -1;
        }
        SDL_QueryTexture(_textures[i].img, NULL, NULL, &_textures[i].w, &_textures[i].h);
        //_textures[i].xscale = 5;
        //_textures[i].yscale = 20;
    }
    // Load skybox
    _skybox.img = render_load_texture(SKYBOX_NAME, &_skybox.pix, &_skybox.pitch);
    if(_skybox.img == NULL)
    {
        // ERROR
        printf("ERROR: Can't load texture %s\n", SKYBOX_NAME);
        return -1;
    }
    SDL_QueryTexture(_skybox.img, NULL, NULL, &_skybox.w, &_skybox.h);
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

    for(int i = 0; i < NUM_TEXTURES; ++i)
    {
        if(_textures[i].img) SDL_DestroyTexture(_textures[i].img);
        if(_textures[i].pix) free(_textures[i].pix);
    }
    if(_skybox.img) SDL_DestroyTexture(_skybox.img);
    if(_skybox.pix) free(_skybox.pix);
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
SDL_Texture *render_load_texture(const char *filename, uint32_t **pix, int *pitch)
{
    SDL_Surface *temp_surface, *optimized_surface;
    SDL_Texture *texture;

    // Switch based on filetype
    if(strstr(filename, ".bmp"))
    {
        temp_surface = SDL_LoadBMP(filename);
    }
    else if(strstr(filename, ".png"))
    {
        temp_surface = IMG_Load(filename);
    }
    else
    {
        printf("Filetype not supported for %s\n", filename);
        return NULL;
    }
    
    if(temp_surface == NULL)
    {
    	printf("Could not load image %s!\nSDL Error: %s\n", filename, IMG_GetError());
    	return NULL;
    }

    // Optimize Surface
    texture = SDL_CreateTextureFromSurface(_renderer, temp_surface);
    if(texture == NULL)
    {
        printf("Can't create texture: %s\n", SDL_GetError());
    }
    // If user wants pixels, give them correct type
    if(pix)
    {
        optimized_surface = SDL_ConvertSurface(temp_surface, _fmt, 0);
        if(optimized_surface == NULL)
        {
            printf("Can't optimize surface: %s\n", SDL_GetError());
            return NULL;
        }
        SDL_SetSurfaceRLE(optimized_surface, 0);
        *pix = malloc(optimized_surface->pitch * optimized_surface->h);
        *pitch = optimized_surface->pitch;
        memcpy((void *)*pix, optimized_surface->pixels, optimized_surface->pitch * optimized_surface->h);
        SDL_FreeSurface(optimized_surface);
    }
    // Free temporary surface
    SDL_FreeSurface(temp_surface);
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

    free(image->pix);
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
    // Set render color to black & clear screen with it
    SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 0xFF);
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

/**
 * Draw a vertical line on the screen using a texture
 * @param[in] x The x position
 * @param[in] y0 The start y
 * @param[in] y1 The end y
 * @param[in] ceil The ceiling y of the wall (lower y)
 * @param[in] floor The floor y of the wall (higher y)
 * @param[in] texture The texture to use
 * @param[in] height The actual height of the sector
 * @param[in] idx The horizontal index to use for the texture
 * @param[in] z The z position of the texture (used for coloring)
 */
int8_t render_vline_textured(uint32_t x, uint32_t y0, uint32_t y1, 
                             int ceil, int floor, 
                             image_t *texture, double height,
                             uint32_t idx, uint16_t z, uint8_t brightness)
{
    // Rectangles!
    SDL_Rect src, dst;
    int h, highest = y1;
    // Set color correction
    uint8_t mod = MIN(z, 0xE0);
    mod = (mod > brightness) ? 0 : brightness - mod;
    SDL_SetTextureColorMod(texture->img, mod, mod, mod);

    src.w = 1;
    dst.w = 1;
    src.x  = idx % texture->w;
    dst.x = x;
    // To avoid any divide-by-zero issues
    if(floor == ceil) {++floor; }

    // Adjust height based on visible height
    height /= texture->yscale;
    // Get screen height for a texture height of 1
    h = PointOnLine(0, ceil, height, floor, 1.0) - ceil;

    dst.y = floor;
    while(dst.y + h > (int)y0 )
    {
        int ca = 0, cb = 0;
        dst.y -= h;
        // Start from the bottom and draw up
        if(dst.y > (int)y1)
        {
            continue;
        }
        if(dst.y + h <= (int)y0)
        {
            break;
        }
        // Clamp the start (and height) if above ceil
        if(dst.y < (int)y0)
        {
            ca = y0 - dst.y;
            dst.y = y0;
        }
        // Clamp end if below cell
        if(dst.y + (h - ca) > highest)
        {
            cb = (dst.y + h) - (ca + highest);
        }
        dst.h = highest - dst.y;
        highest = dst.y;
        if(dst.h < 1) continue;
        src.y = (ca != 0) ? ((double)texture->h * ((double)ca)) / h : 0;
        src.h = ((double)texture->h * dst.h) / h;
        SDL_RenderCopy(_renderer, texture->img, &src, &dst);
    }

    return 1;
}

int8_t render_vline_textured_bitwise(
                            uint32_t x, uint32_t y0, uint32_t y1,
                            int ceil, int floor,
                            image_t *texture, double height,
                            uint32_t idx, uint16_t z, uint8_t brightness)
{
    // Set color correction
    uint8_t mod = MIN(z, 0xE0);
    mod = (mod > brightness) ? 0 : brightness - mod;
    // Set the range for y indices
    int32_t u1 = (height * texture->h) / texture->yscale;
    // Clamp idx
    y0 = MAX(0, y0);
    y1 = MIN(SCR_H - 1, y1);
    idx = idx % texture->w;
    for(uint32_t y = y0; y <= y1; ++y)
    {
        uint8_t r,g,b;
        uint32_t u = PointOnLine(floor, 0, ceil, u1, (int32_t)y) % texture->h;
        // Draw screen point x, y
        SDL_GetRGB(texture->pix[(u * texture->pitch / sizeof(uint32_t)) + idx], _fmt,
                    &r, &g, &b);
        r = r * mod / 0xFF;
        g = g * mod / 0xFF;
        b = b * mod / 0xFF;
        // Draw the point
        _scr_pix[(y * _scr_pitch / sizeof(uint32_t)) + x] = SDL_MapRGBA(_fmt, r,g,b,0xFF);
    }
}                            

int8_t render_point_textured(uint32_t x, uint32_t y, uint32_t z,
                             image_t *texture, 
                             uint32_t tx, uint32_t ty, uint8_t brightness)
{
    // Set color correction
    uint32_t pixel;
    uint8_t r, g, b;
    uint8_t mod = MIN(z, 0xE0);
    mod = (mod > brightness) ? 0 : brightness - mod;

    x = CLAMP(x, 0, SCR_W - 1);
    y = CLAMP(y, 0, SCR_H - 1);

    // Set source x and y with wrapping
    tx = tx % texture->w;
    ty = ty % texture->h;

    // Get colors
    SDL_GetRGB(texture->pix[(ty * texture->pitch / sizeof(uint32_t)) + tx],
                _fmt, &r, &g, &b);
    
    // Apply color mods
    r = r * mod / 0xFF;
    g = g * mod / 0xFF;
    b = b * mod / 0xFF;
    // Draw the point
    _scr_pix[(y * _scr_pitch / sizeof(uint32_t)) + x] = SDL_MapRGBA(_fmt, r,g,b,0xFF);
    return 1;
}

int8_t render_draw_point(uint32_t x, uint32_t y, uint32_t color)
{
    uint8_t r, g, b;
    r = (color & 0xFF0000) >> 16;
    g = (color & 0x00FF00) >> 8;
    b = (color & 0x0000FF);

    SDL_SetRenderDrawColor(_renderer, r, g, b, 0xFF);

    SDL_RenderDrawPoint(_renderer, x, y);

    return 1;
}

/**
 * Toggle debugging for render drawing
 */
void render_toggle_debug(void)
{
    debugging = (debugging) ? 0 : 1;
}

/**
 * Toggle fullscreen mode for the renderer
 */
void render_set_fullscreen(int fs)
{
    SDL_SetWindowFullscreen(_window, (fs) ? SDL_WINDOW_FULLSCREEN : 0);
}

/**
 * Draw the game world
 */
int8_t render_draw_world(void)
{
    // Get game world data
    world_t *world = world_get_world();
    player_t *player = &(world->player);

    //int ytop[SCR_W], ybottom[SCR_W];
    uint32_t i;
    uint32_t bg;
    r_wall_t *wqueue = NULL;
    int numPix;
    void *pix;

    if(ytop == NULL)
    {
        ytop = malloc(SCR_W * sizeof(int));
    }
    if(ybottom == NULL)
    {
        ybottom = malloc(SCR_W * sizeof(int));
    }

    // Start with preprocessing
    wqueue = render_PreProcess(world);

    // Set up occlusion arrays
    for(int i = 0; i < SCR_W; ++i)
    {
        ytop[i] = 0;
        ybottom[i] = SCR_H - 1;
    }

    bg = SDL_MapRGBA(_fmt, 0, 0, 0, 0);
    if(SDL_LockTexture(_screen_buffer, NULL, &pix, &_scr_pitch) != 0)
    {
        printf("Can't stream to texture\n");
    }
    _scr_pix = (uint32_t *)pix;
    //for(i = 0; i < (_scr_pitch / sizeof(uint32_t)) * SCR_H; ++i)
    //{
    //    _scr_pix[i] = bg;
    //}

    // Reset the whole screen
    render_draw_skybox(player);
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
    // Blit screen buffer to the screen
    _scr_pix = NULL;
    SDL_UnlockTexture(_screen_buffer);
    SDL_RenderCopy(_renderer, _screen_buffer, NULL, NULL);

    debugging = 0;
    render_draw_screen();
}