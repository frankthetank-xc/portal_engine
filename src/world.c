/**
 * File to wrap up the game world
 */

/* ***********************************
 * Includes
 * ***********************************/
// My header
#include "world.h"

// Global Headers
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Project headers
#include "common.h"
#include "render.h"
#include "util.h"
#include "input.h"

/* ***********************************
 * Private Definitions
 * ***********************************/

#define BUFLEN 256

#define PLAYER_HEIGHT 6
#define PLAYER_CROUCH_HEIGHT 2.5
#define PLAYER_HEAD_MARGIN 1
#define PLAYER_KNEE_MARGIN 2
#define PLAYER_RADIUS (0.5)

#define PLAYER_MOVE_VEL 0.1
#define PLAYER_BACK_VEL 0.1
#define PLAYER_JUMP_VEL 1.2

#define WALK_MULT (0.4)
#define SPRINT_MULT (2)

#define MOUSE_X_SCALE (-0.01f)
#define MOUSE_Y_SCALE (0.03f)
#define JOY_X_SCALE (-0.07f)
#define JOY_Y_SCALE (0.15f)

/* ***********************************
 * Private Typedefs
 * ***********************************/

/* ***********************************
 * Private variables
 * ***********************************/

static world_t _world;

/* ***********************************
 * Static function prototypes
 * ***********************************/

void world_delete_sector(sector_t *sector);
void world_handle_keys(keys_t *keys);

/* ***********************************
 * Static function implementation
 * ***********************************/

/**
 * Free all data allocated for a sector
 * @note DOES NOT free the sector_t itself!
 * @param[in] sector The sector to free
 */
void world_delete_sector(sector_t *sector)
{
    if(sector == NULL) { return; }
    if(sector->walls != NULL) { free(sector->walls); }
    return;
}

/**
 * Handles all movement and logic based on key inputs
 * @param[in] keys The current keystate
 */
void world_handle_keys(keys_t *keys)
{
    double pi = acos(-1);
    player_t *player = &_world.player;
    int x, y;
    float lx, ly, rx, ry;

    input_get_joystick(&lx, &ly, &rx, &ry);

    /** Move the player */
    if(keys->left)  { player->direction += 0.04; }
    if(keys->right) { player->direction -= 0.04; }
    if(player->direction > (2*pi)) player->direction -= 2 * pi;
    if(player->direction < 0) player->direction += 2 * pi;

    player->velocity.x = 0; player->velocity.y = 0;
    if(fabs(lx) > 0.1 || fabs(ly) > 0.1)
    {
        // Forward/backward
        if(ly < 0)
        {
            player->velocity.x = -(cosf(player->direction) * PLAYER_MOVE_VEL * ly);
            player->velocity.y = -(sinf(player->direction) * PLAYER_MOVE_VEL * ly);
        }
        else
        {
            player->velocity.x = -(cosf(player->direction) * PLAYER_BACK_VEL * ly);
            player->velocity.y = -(sinf(player->direction) * PLAYER_BACK_VEL * ly);
        }
        
        // Left/right
        player->velocity.x +=  (sinf(player->direction) * PLAYER_MOVE_VEL * lx);
        player->velocity.y += -(cosf(player->direction) * PLAYER_MOVE_VEL * lx);
    }
    else
    {
        if(keys->w || keys->up)
        { 
            player->velocity.x += (cosf(player->direction) * PLAYER_MOVE_VEL);
            player->velocity.y += (sinf(player->direction) * PLAYER_MOVE_VEL);
        }
        if(keys->s || keys->down)
        {
            player->velocity.x += -(cosf(player->direction) * PLAYER_BACK_VEL);
            player->velocity.y += -(sinf(player->direction) * PLAYER_BACK_VEL);
        }
        if(keys->a)
        { 
            player->velocity.x += -(sinf(player->direction) * PLAYER_MOVE_VEL);
            player->velocity.y +=  (cosf(player->direction) * PLAYER_MOVE_VEL);
        }
        if(keys->d)
        {
            player->velocity.x +=  (sinf(player->direction) * PLAYER_MOVE_VEL);
            player->velocity.y += -(cosf(player->direction) * PLAYER_MOVE_VEL);
        }
    }
    
    if(keys->space)
    {
        // If player is on the ground, let him JUMP
        if(FEQ(player->pos.z,_world.sectors[player->sector].floor))
        {
            player->velocity.z = 1.2;
        }
    }

    // Handle crouching
    if(keys->c)
    {
        if(player->height > PLAYER_CROUCH_HEIGHT)
        {
            player->height = MAX(player->height - 0.5, PLAYER_CROUCH_HEIGHT);
        }
    }
    else
    {
        if(player->height < PLAYER_HEIGHT)
        {
            sector_t *sect = &_world.sectors[player->sector];
            player->height = MIN(player->height + 0.5, MIN(PLAYER_HEIGHT, sect->ceil - (player->pos.z + player->headmargin)) );
        }
    }
    
    if(player->height < PLAYER_HEIGHT)
    {
        player->velocity.x *= WALK_MULT;
        player->velocity.y *= WALK_MULT;
    }
    if(keys->shift)
    {
        player->velocity.x *= SPRINT_MULT;
        player->velocity.y *= SPRINT_MULT;
    }

    //if(keys->down) { player->yaw += 0.1; }
    //if(keys->up) { player->yaw -= 0.1; }
    //player->yaw = CLAMP(player->yaw, -MAX_YAW, MAX_YAW);

    // Get look info
    mouse_get_input(&x, &y);
    if(fabs(rx) > 0.05 || fabs(ry) > 0.05)
    {
        player->direction +=  rx * JOY_X_SCALE;
        player->yaw = CLAMP(player->yaw + ry*JOY_Y_SCALE, -MAX_YAW, MAX_YAW);
    }
    else
    {
        player->direction += x * MOUSE_X_SCALE;
        player->yaw = CLAMP(player->yaw + y*MOUSE_Y_SCALE, -MAX_YAW, MAX_YAW);
    }
    
}

/* ***********************************
 * Public function implementation
 * ***********************************/

/**
 * FORMAT OF INPUT FILE
 * 
 * VERTICES:
 * v [id] [x] [y]
 * 
 * SECTORS:
 * s [id] [floor] [ceiling] [brightness] [number of vectors] [list of vector ID] [list of sector ID portals]
 * 
 * PLAYER:
 * p [x] [y] [sector ID]
 *
 */

/**
 * Load specified input file
 * @param[in] filename The name of the world file to load
 * @return 0 on success
 */
int8_t world_load(const char *filename)
{
    FILE *fp = fopen(filename, "rt");
    char buf[BUFLEN * 4], word[BUFLEN], *ptr;
    int n, i, rc, numVertices = 0;
    xy_t *vert;
    sector_t *sect;

    if(fp == NULL)
    {
        printf("No file %s\n", filename);
        return -1;
    }

    _world.numSectors = 0;
    _world.sectors = NULL;
    _world.vertices = NULL;

    // Set default values for player
    _world.player.pos.x = 0;
    _world.player.pos.y = 0;
    _world.player.sector = 0;
    _world.player.yaw           = 0;
    _world.player.direction     = 0;
    _world.player.headmargin    = PLAYER_HEAD_MARGIN;
    _world.player.height        = PLAYER_HEIGHT;
    _world.player.kneemargin    = PLAYER_KNEE_MARGIN;
    _world.player.velocity.x = 0;
    _world.player.velocity.y = 0;
    _world.player.velocity.z = 0;

    // Read the file
    while(fgets(buf, sizeof(buf), fp))
    {
        rc = sscanf(buf, "%32s%n", word, &n);
        ptr = buf + n;
        switch((rc > 0) ? word[0] : '\0')
        {
            case 'v':   // Vertex
                _world.vertices = realloc(_world.vertices, (++numVertices) * sizeof(xy_t));
                vert = &(_world.vertices[numVertices - 1]);
                // Read in: ID X Y
                rc = sscanf(ptr, "%*i %lf %lf", &(vert->x), &(vert->y));
                break;
            case 's':   // Sector
                
                _world.sectors = realloc(_world.sectors, (++(_world.numSectors)) * sizeof(sector_t));
                sect = &(_world.sectors[_world.numSectors - 1]);

                // Read in: ID Floor Ceiling NumVertices
                rc = sscanf(ptr, "%*i %lf %lf %hi %hi %hhi %hu%n", &sect->floor, &sect->ceil,
                                                            &sect->texture_floor, &sect->texture_ceil,
                                                            &sect->brightness, &sect->num_walls, &n);
                ptr += n;

                // TODO load textures
                //sect->texture_floor = TEXTURE_MOSS;
                //sect->texture_ceil  = TEXTURE_SMOOTHSTONE;

                sect->walls = (wall_t *)malloc(sect->num_walls * sizeof(wall_t));

                // Read in all walls
                for(i = 0; i < sect->num_walls; ++i)
                {
                    // Get vertices
                    rc = sscanf(ptr, "%i %i%n", &sect->walls[i].v0, &sect->walls[i].v1, &n);
                    ptr += n;
                    // Get neighbor
                    rc = sscanf(ptr, "%32s%n", word, &n);
                    ptr += n;
                    // Value of 'x' means no neighbor
                    if(word[0] == 'x')
                    {
                        sect->walls[i].neighbor = -1;
                    }
                    else
                    {
                        sscanf(word, "%i", &(sect->walls[i].neighbor));
                    }
                    // Get textures
                    rc = sscanf(ptr, "%hi %hi %hi%n", &sect->walls[i].texture_low,
                                                   &sect->walls[i].texture_mid, 
                                                   &sect->walls[i].texture_high,
                                                   &n);
                    ptr += n;
                    //sect->walls[i].texture_low  = TEXTURE_SMOOTHSTONE;
                    //sect->walls[i].texture_mid  = TEXTURE_RUSTYSHEET;
                    //sect->walls[i].texture_high = TEXTURE_BRICK;
                }
                break;
            case 'p':   // Player
                // Read in: x y sector
                sscanf(ptr, "%lf %lf %i", &_world.player.pos.x, &_world.player.pos.y, &_world.player.sector);
                break;
            default:
                break;
        }
    }

    // Set player height
    _world.player.pos.z = _world.sectors[_world.player.sector].floor;

    fclose(fp);

    return 0;
}

/**
 * Free all memory related to the world
 */
void world_close(void)
{
    // Free vertex array
    if(_world.vertices) free(_world.vertices);
    if(_world.sectors)
    {
        // Iterate through each sector and free its data
        for(int i = 0; i < _world.numSectors; ++i)
        {
            world_delete_sector(&_world.sectors[i]);
        }
        free(_world.sectors);
    }

    return;
}

/**
 * Handle logic of moving player
 */
void world_move_player(keys_t *keys)
{
    player_t *player = &_world.player;
    float px = player->pos.x, py = player->pos.y;
    int done, newsector = 0;;
    // Construct vector for player movement
    float dx = player->velocity.x, dy = player->velocity.y;
    xy_t dest = {px+dx, py+dy};
    xy_t farDest;
    xy_t ppos = {px, py};
    sector_t *sect;
    int prev_sect = -1;

    // Check user keys
    world_handle_keys(keys);

    // Horizontal movement loop
    if(dx != 0.0 || dy != 0.0)
    {
        do
        {
            sect = &_world.sectors[_world.player.sector];
            done = 1;
            // Collision detection
            for(uint16_t s = 0; s < sect->num_walls; ++s)
            {
                // Get the neighbor of this cell
                int32_t neighbor = sect->walls[s].neighbor;
                sector_t *nsect = (neighbor < 0) ? NULL : &_world.sectors[neighbor];

                xy_t *v0 = &_world.vertices[sect->walls[s].v0];
                xy_t *v1 = &_world.vertices[sect->walls[s].v1];
                dest.x = px+dx; dest.y = py+dy;
                farDest.x = dest.x + ((dx > 0) ? PLAYER_RADIUS : -PLAYER_RADIUS);
                farDest.y = dest.y + ((dy > 0) ? PLAYER_RADIUS : -PLAYER_RADIUS);

                // Check if player is about to cross an edge
                float hole_low  = neighbor < 0 ?  9e9 : MAX(sect->floor, nsect->floor);
                float hole_high = neighbor < 0 ? -9e9 : MIN(sect->ceil,  nsect->ceil);
                if((neighbor < 0) || hole_high < (player->pos.z + player->height + player->headmargin)
                    || (hole_low > player->pos.z + player->kneemargin))
                {
                    // Compare far line
                    if(lines_intersect(&ppos, &farDest, v0, v1)
                       && (!world_inside_sector(&farDest, sect)))
                    {
                        // Player can't fit, project their vector along the wall
                        // TODO this isn't super efficient (that sqrt call
                        // is particularly nasty), but it's called
                        // rarely enough that it doesn't really matter....
                        project_vector(dx,dy, (v1->x - v0->x), (v1->y - v0->y), &dx, &dy);
                    }
                }
                else if(lines_intersect(&ppos, &dest, v0, v1)
                        && (!world_inside_sector(&dest, sect)))
                {
                    // Player can fit - update active sector
                    if(prev_sect == neighbor) continue;
                    if(!world_inside_sector(&dest, &_world.sectors[neighbor])) continue;
                    prev_sect = player->sector;
                    player->sector = neighbor;
                    newsector = 1;
                    done = 0;
                }

            }
        } while (!done);

        sect = &_world.sectors[_world.player.sector];

        if(!newsector)
        {
            // Check position one last time. If the player is
            // about to escape a sector into space, cancel the move
            dest.x = px+dx; dest.y = py+dy;
            if(!world_inside_sector(&dest, sect))
            {
                dx = 0; dy = 0;
            }
        }

        // Now move the player
        player->pos.x += dx;
        player->pos.y += dy;
    }
    
    sect = &_world.sectors[_world.player.sector];

    // Vertical movement
    if(player->pos.z > sect->floor)
    {
        // Add gravity
        player->velocity.z -= 0.05;
    }
    player->pos.z += player->velocity.z;

    if(player->pos.z < sect->floor)
    {
        player->pos.z = sect->floor;
        player->velocity.z = 0;
    }
    if(player->pos.z + player->height + player->headmargin > sect->ceil)
    {
        player->pos.z = sect->ceil - player->height - player->headmargin;
        player->velocity.z = 0;
    }
}

/**
 * Updates the sector that the player is currently in
 */
void world_player_update_sector()
{
    float px = _world.player.pos.x, py = _world.player.pos.y;
    sector_t *sect = &_world.sectors[_world.player.sector];
    // First, check if player is still in their current sector
    
}

/**
 * Algorithm to test if a point is inside a sector. 
 * Draw a line west from the point. If it intersects an odd number
 * of edges, it is inside the sector. If it intersects an even number
 * of edges, it is outside the sector.
 * 
 * Based on how the Build engine checks if a point is within a sector.
 * 
 * @param[in] x The x coordinate
 * @param[in] y The y coordinate
 * @param[in] sect The sector
 */
int world_inside_sector(xy_t *p, sector_t *sect)
{
    float dx, x;
    uint16_t count = 0;
    for(int i = 0; i < sect->num_walls; ++i)
    {
        // Get the vertices for this edge
        xy_t *v0 = &_world.vertices[sect->walls[i].v0], *v1 = &_world.vertices[sect->walls[i].v1];

        // Check that vertices contain the right y position
        if(p->y > MAX(v0->y,v1->y) || p->y <= MIN(v0->y,v1->y)) continue;
        // Get the x position at this y position
        dx = (v1->x - v0->x) / (v1->y - v0->y);
        x  = (v0->x) + (dx * (p->y - v0->y));

        if(x < p->x) ++count;
    }

    return (count & 0x1) ? 1 : 0;
}

/**
 * Returns handle to static world object
 */
world_t *world_get_world(void)
{
    return &_world;
}