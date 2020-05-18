/**
 * Generic mob implementation.
 */

/* ***********************************
 * Includes
 * ***********************************/
// My header
#include "mob.h"

// Global headers

// Project headers
#include "util.h"
#include "world.h"

/* ***********************************
 * Private Definitions
 * ***********************************/
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

/**
 * Static array of mob configuration data
 */
static const mob_conf_t mob_conf_data[MOB_TYPE_NUMBER] = 
{
    // Player
    {
        .height = PLAYER_HEIGHT,
        .kneemargin = PLAYER_KNEE_MARGIN,
        .eyemargin = PLAYER_HEAD_MARGIN,
        .max_health = 100,
    },
    // Enemy 1
    {
        .height = PLAYER_HEIGHT,
        .kneemargin = PLAYER_KNEE_MARGIN,
        .eyemargin = PLAYER_HEAD_MARGIN,
        .max_health = 100,
    }
};

/* ***********************************
 * Static function prototypes
 * ***********************************/

/* ***********************************
 * Static function implementation
 * ***********************************/

/* ***********************************
 * Public function implementation
 * ***********************************/

/**
 * Initializes a mob structure based on the mob type
 */
void mob_init(mob_t *mob, mob_type_t type)
{
    if(mob == NULL)
    {
        return;
    }

    // Get config data
    mob_conf_t const *conf = &mob_conf_data[(uint32_t)type];
    mob->height     = conf->height;
    mob->kneemargin = conf->kneemargin;
    mob->eyemargin  = conf->eyemargin;
    mob->health     = conf->max_health;
    // Initialize position data
    mob->velocity.x = 0;
    mob->velocity.y = 0;
    mob->velocity.z = 0;
    // If this is the player, intialize their data
    mob->player = NULL;
    if(type == MOB_TYPE_PLAYER)
    {
        mob->player = malloc(sizeof(player_t));
    }

    return;
}

/**
 * Updates a mob's position based on the current velocity vector
 */
void mob_pos_update(mob_t *mob)
{
    if(mob == NULL) return;

    float px = mob->pos.x, py = mob->pos.y;
    int done, newsector = 0;;
    // Construct vector for movement
    float dx = mob->velocity.x, dy = mob->velocity.y;
    xy_t dest = {px+dx, py+dy};
    xy_t farDest;
    xy_t ppos = {px, py};
    sector_t *sect;
    int prev_sect = -1;

    // Horizontal movement loop
    if(dx != 0.0 || dy != 0.0)
    {
        do
        {
            sect = world_get_sector(mob->sector);
            done = 1;
            // Collision detection
            for(uint16_t s = 0; s < sect->num_walls; ++s)
            {
                // Get the neighbor of this cell
                int32_t neighbor = sect->walls[s].neighbor;
                sector_t *nsect = (neighbor < 0) ? NULL : world_get_sector(neighbor);

                xy_t *v0 = world_get_vertex(sect->walls[s].v0);
                xy_t *v1 = world_get_vertex(sect->walls[s].v1);
                dest.x = px+dx; dest.y = py+dy;
                farDest.x = dest.x + ((dx > 0) ? PLAYER_RADIUS : -PLAYER_RADIUS);
                farDest.y = dest.y + ((dy > 0) ? PLAYER_RADIUS : -PLAYER_RADIUS);

                // Check if mob is about to cross an edge
                float hole_low  = neighbor < 0 ?  9e9 : MAX(sect->floor, nsect->floor);
                float hole_high = neighbor < 0 ? -9e9 : MIN(sect->ceil,  nsect->ceil);
                if((neighbor < 0) || hole_high < (mob->pos.z + mob->height + mob->eyemargin)
                    || (hole_low > mob->pos.z + mob->eyemargin))
                {
                    // Compare far line
                    if(lines_intersect(&ppos, &farDest, v0, v1)
                       && (!world_inside_sector(&farDest, sect)))
                    {
                        // Mob can't fit, project their vector along the wall
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
                    if(!world_inside_sector(&dest, nsect)) continue;
                    prev_sect = mob->sector;
                    mob->sector = neighbor;
                    newsector = 1;
                    done = 0;
                }

            }
        } while (!done);

        sect = world_get_sector(mob->sector);

        if(!newsector)
        {
            // Check position one last time. If the mob is
            // about to escape a sector into space, cancel the move
            dest.x = px+dx; dest.y = py+dy;
            if(!world_inside_sector(&dest, sect))
            {
                dx = 0; dy = 0;
            }
        }

        // Now move the mob
        mob->pos.x += dx;
        mob->pos.y += dy;
    }
    
    sect = world_get_sector(mob->sector);

    // Vertical movement
    if(mob->pos.z > sect->floor)
    {
        // Add gravity
        mob->velocity.z -= 0.05;
    }
    mob->pos.z += mob->velocity.z;

    if(mob->pos.z < sect->floor)
    {
        mob->pos.z = sect->floor;
        mob->velocity.z = 0;
    }
    if(mob->pos.z + mob->height + mob->eyemargin > sect->ceil)
    {
        mob->pos.z = sect->ceil - mob->height - mob->eyemargin;
        mob->velocity.z = 0;
    }
}