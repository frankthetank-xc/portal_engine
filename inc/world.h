/**
 * File to wrap up the game world
 */

#ifndef __WORLD_H__
#define __WORLD_H__

#include "common.h"
#include "input.h"

/* ***********************************
 * Public Definitions
 * ***********************************/

/** Max number of sectors */
#define MAX_SECTORS (1024)

/* ***********************************
 * Public Typedefs
 * ***********************************/

typedef struct sector_struct
{
    float floor, ceil;
    // Number of vertices
    uint16_t num_vert;
    // Array of vertices, stored as vertex IDs
    // Vertices are in clockwise order!
    int32_t *vertices;
    // List of neighbor sectors, stored as sector IDs
    // Neighbors are in the same order as vertices!
    int32_t *neighbors;
} sector_t;

typedef struct player_struct
{
    xyz_t pos, velocity;
    float direction, yaw, height, headmargin, kneemargin;
    uint32_t sector;
} player_t;

typedef struct world_struct
{
    xy_t     *vertices;
    sector_t *sectors;
    uint32_t numSectors;

    player_t player;
} world_t;

/* ***********************************
 * Public Functions
 * ***********************************/
int8_t world_load(const char *filename);
void world_destroy(void);

// Player helper functions
void world_move_player(keys_t *keys);
void world_player_update_sector();

world_t *world_get_world(void);

#endif /*__WORLD_H__*/