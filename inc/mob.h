/**
 * Generic mob definition. 
 * Mobs are any object in the game world (static or living), 
 * as well as players.
 */

#ifndef __MOB_H__
#define __MOB_H__

#include "common.h"
/* ***********************************
 * Public Definitions
 * ***********************************/

/* ***********************************
 * Public Typedefs
 * ***********************************/

typedef enum mob_type_enum
{
    MOB_TYPE_PLAYER,
    MOB_TYPE_ENEMY1,
    MOB_TYPE_NUMBER
} mob_type_t;

/**
 * Configuration data for each type of mob. This is
 * hard coded for each type of mob
 */
typedef struct mob_config_struct
{
    // Height, knee, eyes
    double height, kneemargin, eyemargin;
    uint32_t max_health;
    // TODO animation information (textures etc)
} mob_conf_t;

typedef struct player_struct
{
    // Current sector as sector ID
    double yaw;
} player_t;

typedef struct mob_struct
{
    // Position and velocity info
    xyz_t pos, velocity;
    // Direction
    double direction;
    // Current sector
    uint32_t sector;
    // Health
    uint32_t health;
    // Config info
    double height, kneemargin, eyemargin;
    // Optional additional data for a player
    player_t *player;
} mob_t;

/* ***********************************
 * Public Functions
 * ***********************************/

// Structure init
void mob_init(mob_t *mob, mob_type_t type);

// Update position based on computed velocity
void mob_pos_update(mob_t *mob);

#endif /* __MOB_H__ */