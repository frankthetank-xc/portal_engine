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
#include "player.h"

/* ***********************************
 * Private Definitions
 * ***********************************/
#define BUFLEN 256

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

    mob_init(&_world.player, MOB_TYPE_PLAYER);

    // Set default values for player
    _world.player.pos.x = 0;
    _world.player.pos.y = 0;
    _world.player.sector = 0;

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

void world_tick(keys_t *keys)
{
    // Check user keys
    player_handle_input(&_world.player, keys);

    // Update player position
    mob_pos_update(&_world.player);
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

/**
 * Returns handle to sector if the ID is valid
 */
sector_t *world_get_sector(uint32_t id)
{
    if(id < _world.numSectors)
    {
        return &_world.sectors[id];
    }
}

/**
 * Returns handle to vertex
 * @note There is no bounds checking
 */
xy_t *world_get_vertex(uint32_t id)
{
    return &_world.vertices[id];
}