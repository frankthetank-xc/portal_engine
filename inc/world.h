/**
 * File to wrap up the game world
 */

#ifndef __WORLD_H__
#define __WORLD_H__

#include "common.h"
#include "render.h"
#include "input.h"

/* ***********************************
 * Public Definitions
 * ***********************************/

/** Max number of sectors */
#define MAX_SECTORS (1024)

#define MAX_YAW (5.0)

/* ***********************************
 * Public Typedefs
 * ***********************************/

typedef struct wall_struct
{
    int32_t v0, v1;
    int32_t neighbor;
    int16_t texture_low, texture_mid, texture_high;
} wall_t;

typedef struct sector_struct
{
    // Height values for floor/ceiling
    double floor, ceil;
    uint16_t num_walls;
    // Array of walls, stored in CLOCKWISE order
    wall_t *walls;
    // Brightness: 0 = pitch black, 255 = fully bright
    uint8_t brightness;
    int16_t texture_floor, texture_ceil;
} sector_t;

typedef struct player_struct
{
    // Current position + velocity vectors
    xyz_t pos, velocity;
    // Direction and yaw as angles; height, headmargin, and kneemargin as heightvalues
    double direction, yaw, height, headmargin, kneemargin;
    // Current sector as sectorID
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
void world_close(void);

// Player helper functions
void world_move_player(keys_t *keys);
void world_player_update_sector();
int world_inside_sector(xy_t *p, sector_t *sect);

world_t *world_get_world(void);

#endif /*__WORLD_H__*/