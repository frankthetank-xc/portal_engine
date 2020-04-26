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
#define PLAYER_HEAD_MARGIN 1
#define PLAYER_KNEE_MARGIN 2

#define PLAYER_MOVE_VEL 0.2
#define PLAYER_BACK_VEL 0.1
#define PLAYER_JUMP_VEL 1.2

#define MOUSE_X_SCALE (0.01f)
#define MOUSE_Y_SCALE (0.03f)

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
int inside_sector(xy_t *p, sector_t *sect);

void print_debug(void);

/* ***********************************
 * Static function implementation
 * ***********************************/

void world_delete_sector(sector_t *sector)
{
    if(sector == NULL) { return; }

    if(sector->vertices != NULL) { free(sector->vertices); }
    if(sector->neighbors != NULL) { free(sector->neighbors); }

    return;
}

void world_handle_keys(keys_t *keys)
{
    double pi = acos(-1);
    player_t *player = &_world.player;
    int x, y;

    /** Move the player */
    if(keys->left)  { player->direction -= 0.02; }
    if(keys->right) { player->direction += 0.02; }
    CLAMP(player->direction, 0.0, 2 * pi);

    player->velocity.x = 0; player->velocity.y = 0;
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
    if(keys->d)
    { 
        player->velocity.x += -(sinf(player->direction) * PLAYER_MOVE_VEL);
        player->velocity.y +=  (cosf(player->direction) * PLAYER_MOVE_VEL);
    }
    if(keys->a)
    {
        player->velocity.x +=  (sinf(player->direction) * PLAYER_MOVE_VEL);
        player->velocity.y += -(cosf(player->direction) * PLAYER_MOVE_VEL);
    }
    if(keys->space)
    {
        // If player is on the ground, let him JUMP
        if(FEQ(player->pos.z,_world.sectors[player->sector].floor))
        {
            player->velocity.z = 1.2;
        }
    }

    //if(keys->down) { player->yaw += 0.1; }
    //if(keys->up) { player->yaw -= 0.1; }
    //player->yaw = CLAMP(player->yaw, -5, 5);

    // Get mouse info
    mouse_get_input(&x, &y);
    player->direction += x * MOUSE_X_SCALE;
    player->yaw = CLAMP(player->yaw + y*MOUSE_Y_SCALE, -5, 5);
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
int inside_sector(xy_t *p, sector_t *sect)
{
    float dx, x;
    uint16_t count = 0;
    for(int i = 0; i < sect->num_vert; ++i)
    {
        // Get the vertices for this edge
        xy_t *v0 = &_world.vertices[sect->vertices[i]], *v1 = &_world.vertices[sect->vertices[i+1]];

        // Check that vertices contain the right y position
        if(p->y > MAX(v0->y,v1->y) || p->y < MIN(v0->y,v1->y)) continue;
        // Get the x position at this y position
        dx = (v1->x - v0->x) / (v1->y - v0->y);
        x  = (v0->x) + (dx * (p->y - v0->y));

        if(x < p->x) ++count;
    }

    return (count & 0x1) ? 1 : 0;
}

void print_debug(void)
{
    player_t *player = &_world.player;

    printf("\rPlayer pos %5.2f%5.2f%5.2f ", player->pos.x, player->pos.y, player->pos.z);
    printf("angle %4.2f", player->direction);
    fflush(stdout);
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
 * s [id] [floor] [ceiling] [number of vectors] [list of vector ID] [list of sector ID portals]
 * 
 * PLAYER:
 * p [x] [y] [sector ID]
 *
 */

/**
 * Load specified input file
 */
int8_t world_load(const char *filename)
{
    FILE *fp = fopen(filename, "rt");
    char buf[BUFLEN], word[BUFLEN], *ptr;
    int n, i, rc, numVertices = 0, sectVertices;
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
            // Vertex
            case 'v':
                _world.vertices = realloc(_world.vertices, (++numVertices) * sizeof(xy_t));
                vert = &(_world.vertices[numVertices - 1]);
                // Read in: ID X Y
                rc = sscanf(ptr, "%*i %f %f", &(vert->x), &(vert->y));
                break;
            // Sector
            case 's':
                
                _world.sectors = realloc(_world.sectors, (++(_world.numSectors)) * sizeof(sector_t));
                sect = &(_world.sectors[_world.numSectors - 1]);

                // Read in: ID Floor Ceiling NumVertices
                rc = sscanf(ptr, "%*i %f %f %i%n", &sect->floor, &sect->ceil, &sectVertices, &n);
                ptr += n;

                sect->num_vert = sectVertices;

                sect->vertices = (int32_t *)malloc((sectVertices + 1) * sizeof(int32_t));
                sect->neighbors = (int32_t *)malloc(sectVertices * sizeof(int32_t));

                // Read in all vertices
                for(i = 0; i < sectVertices; ++i)
                {
                    rc = sscanf(ptr, "%i%n", &sect->vertices[i], &n);
                    ptr += n;
                }

                // Copy the first vertex to the last to make the sector loop
                sect->vertices[sectVertices] = sect->vertices[0];

                // Read in all neighbors
                for(i = 0; i < sectVertices; ++i)
                {
                    rc = sscanf(ptr, "%32s%n", word, &n);
                    ptr += n;

                    if(word[0] == 'x')
                    {
                        sect->neighbors[i] = -1;
                    }
                    else
                    {
                        sscanf(word, "%i", &(sect->neighbors[i]));
                    }
                    
                }
                break;
            
            // Player
            case 'p':
                // Read in: x y sector
                sscanf(ptr, "%f %f %i", &_world.player.pos.x, &_world.player.pos.y, &_world.player.sector);
                break;
            default:
                break;
        }
    }

    // Set player height
    _world.player.pos.z = _world.sectors[_world.player.sector].floor;

    fclose(fp);
}

/**
 * Free all memory related to the world
 */
void world_destroy(void)
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
    int done;
    // Construct vector for player movement
    float dx = player->velocity.x, dy = player->velocity.y;
    xy_t dest = {px+dx, py+dy};
    xy_t ppos = {px, py};
    sector_t *sect;

    // Check user keys
    world_handle_keys(keys);

    // Horizontal movement loop
    if(dx != 0.0 || dy != 0.0)
    {
        do
        {
            sect = &_world.sectors[_world.player.sector];
            done = 1;

            //if(!inside_sector(&ppos,sect)) printf("Player not inside sector?\n");

            // Collision detection
            for(uint16_t s = 0; s < sect->num_vert; ++s)
            {
                // Get the neighbor of this cell
                int32_t neighbor = sect->neighbors[s];
                sector_t *nsect = (neighbor < 0) ? NULL : &_world.sectors[neighbor];

                xy_t *v0 = &_world.vertices[sect->vertices[s+0]];
                xy_t *v1 = &_world.vertices[sect->vertices[s+1]];
                dest.x = px+dx; dest.y = py+dy;

                // Check if player is about to cross an edge
                //if(IntersectBox(px,py, px+dx, py+dy, v0->x, v0->y, v1->x, v1->y)
                //    && PointSide(px+dx, py+dy, v0->x, v0->y, v1->x, v1->y) < 0)
                if(lines_intersect(&ppos, &dest, v0, v1)
                  && (!inside_sector(&dest, sect)))
                {
                    float hole_low  = neighbor < 0 ?  9e9 : MAX(sect->floor, nsect->floor);
                    float hole_high = neighbor < 0 ? -9e9 : MIN(sect->ceil,  nsect->ceil);

                    if((hole_high < (player->pos.z + player->height + player->headmargin))
                    || (hole_low > player->pos.z + player->kneemargin))
                    {
                        // Player can't fit, project their vector along the wall
                        // TODO this isn't super efficient (that sqrt call
                        // is particularly nasty), but it's called
                        // rarely enough that it doesn't really matter....
                        project_vector(dx,dy, (v1->x - v0->x), (v1->y - v0->y), &dx, &dy);
                    }
                    else
                    {
                        // Player can fit - update active sector
                        player->sector = neighbor;
                        //printf("New sector %i\n", neighbor);
                        done = 0;
                    }
                }

            }
        } while (!done);

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

    //print_debug();
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
 * Returns handle to static world object
 */
world_t *world_get_world(void)
{
    return &_world;
}