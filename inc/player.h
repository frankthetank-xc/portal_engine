/**
 * Player control definition
 */

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "common.h"
#include "mob.h"
#include "input.h"
/* ***********************************
 * Public Definitions
 * ***********************************/

/* ***********************************
 * Public Typedefs
 * ***********************************/

/* ***********************************
 * Public Functions
 * ***********************************/

// Handle input
void player_handle_input(mob_t *player, keys_t *keys);

#endif /*__PLAYER_H__*/