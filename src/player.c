/**
 * Player control implementation
 */

/* ***********************************
 * Includes
 * ***********************************/
// My header
#include "player.h"

// Global Headers
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Project headers
#include "common.h"
#include "render.h"
#include "util.h"
#include "input.h"
#include "world.h"

/* ***********************************
 * Private Definitions
 * ***********************************/

#define PLAYER_HEIGHT 6
#define PLAYER_CROUCH_HEIGHT 2.5
#define PLAYER_HEAD_MARGIN 1
#define PLAYER_KNEE_MARGIN 2
#define PLAYER_RADIUS (0.5)

#define PLAYER_MOVE_VEL 0.05
#define PLAYER_BACK_VEL 0.05
#define PLAYER_JUMP_VEL 1.2
#define PLAYER_FRICTION (0.7)
#define PLAYER_AIR_MULT (0.3)
#define MAX_SPEED (0.3)

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

/* ***********************************
 * Static function prototypes
 * ***********************************/

/* ***********************************
 * Static function implementation
 * ***********************************/

/* ***********************************
 * Public function implementation
 * ***********************************/

void player_handle_input(mob_t *player, keys_t *keys)
{
    if(player == NULL || keys == NULL) { return; }
    double pi = acos(-1);
    int x, y;
    float lx, ly, rx, ry;
    sector_t *sect = world_get_sector(player->sector);
    xy_t vel;

    input_get_joystick(&lx, &ly, &rx, &ry);

    /** Move the player */
    if(keys->left)  { player->direction += 0.04; }
    if(keys->right) { player->direction -= 0.04; }
    if(player->direction > (2*pi)) player->direction -= 2 * pi;
    if(player->direction < 0) player->direction += 2 * pi;

    vel.x = 0; vel.y = 0;
    if(fabs(lx) > 0.1 || fabs(ly) > 0.1)
    {
        // Forward/backward
        if(ly < 0)
        {
            vel.x = -(cosf(player->direction) * PLAYER_MOVE_VEL * ly);
            vel.y = -(sinf(player->direction) * PLAYER_MOVE_VEL * ly);
        }
        else
        {
            vel.x = -(cosf(player->direction) * PLAYER_BACK_VEL * ly);
            vel.y = -(sinf(player->direction) * PLAYER_BACK_VEL * ly);
        }
        
        // Left/right
        vel.x +=  (sinf(player->direction) * PLAYER_MOVE_VEL * lx);
        vel.y += -(cosf(player->direction) * PLAYER_MOVE_VEL * lx);
    }
    else
    {
        if(keys->w || keys->up)
        { 
            vel.x += (cosf(player->direction) * PLAYER_MOVE_VEL);
            vel.y += (sinf(player->direction) * PLAYER_MOVE_VEL);
        }
        if(keys->s || keys->down)
        {
            vel.x += -(cosf(player->direction) * PLAYER_BACK_VEL);
            vel.y += -(sinf(player->direction) * PLAYER_BACK_VEL);
        }
        if(keys->a)
        { 
            vel.x += -(sinf(player->direction) * PLAYER_MOVE_VEL);
            vel.y +=  (cosf(player->direction) * PLAYER_MOVE_VEL);
        }
        if(keys->d)
        {
            vel.x +=  (sinf(player->direction) * PLAYER_MOVE_VEL);
            vel.y += -(cosf(player->direction) * PLAYER_MOVE_VEL);
        }
    }
    
    if(keys->space)
    {
        // If player is on the ground, let him JUMP
        if(FEQ(player->pos.z, sect->floor))
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
            
            player->height = MIN(player->height + 0.5, MIN(PLAYER_HEIGHT, sect->ceil - (player->pos.z + player->eyemargin)) );
        }
    }
    
    if(player->height < PLAYER_HEIGHT)
    {
        vel.x *= WALK_MULT;
        vel.y *= WALK_MULT;
    }
    if(keys->shift)
    {
        vel.x *= SPRINT_MULT;
        vel.y *= SPRINT_MULT;
    }

    //if(keys->down) { player->yaw += 0.1; }
    //if(keys->up) { player->yaw -= 0.1; }
    //player->yaw = CLAMP(player->yaw, -MAX_YAW, MAX_YAW);

    // Apply velocity to player's vectors ONLY if they're on the ground
    if(player->pos.z < (sect->floor + 0.5) || 1)
    {
        player->velocity.x = (player->velocity.x * PLAYER_FRICTION) + vel.x;
        player->velocity.y = (player->velocity.y * PLAYER_FRICTION) + vel.y;
    }
    else
    {
        player->velocity.x = (player->velocity.x * (PLAYER_FRICTION)) + (vel.x * (PLAYER_AIR_MULT));
        player->velocity.y = (player->velocity.y * (PLAYER_FRICTION)) + (vel.y * (PLAYER_AIR_MULT));
    }

    player->velocity.x = CLAMP(player->velocity.x, -MAX_SPEED, MAX_SPEED);
    player->velocity.y = CLAMP(player->velocity.y, -MAX_SPEED, MAX_SPEED);

    // Get look info
    mouse_get_input(&x, &y);
    if(fabs(rx) > 0.05 || fabs(ry) > 0.05)
    {
        player->direction +=  rx * JOY_X_SCALE;
        player->player->yaw = CLAMP(player->player->yaw + ry*JOY_Y_SCALE, -MAX_YAW, MAX_YAW);
    }
    else
    {
        player->direction += x * MOUSE_X_SCALE;
        player->player->yaw = CLAMP(player->player->yaw + y*MOUSE_Y_SCALE, -MAX_YAW, MAX_YAW);
    }
    
}