/**
 * Playground program to test SDL features
 */

// Global headers
#include <stdio.h>
#include <stdint.h>
#include <SDL.h>
#include <math.h>

// Project headers
#include "render.h"
#include "common.h"
#include "world.h"
#include "input.h"

/* ***********************************
 * Private Defines
 * ***********************************/
#define MOVE_LEN (1)

/* ***********************************
 * Static Typedef
 * ***********************************/


/* ***********************************
 * Static Variables
 * ***********************************/

/* ***********************************
 * Static function prototypes
 * ***********************************/

/* ***********************************
 * Static function implementation
 * ***********************************/

/* ***********************************
 * Main function
 * ***********************************/
int main(int argc, char *argv[])
{
    image_t sprite;
    int done = 0, moving;
    world_t *world;
    player_t *player;
    double pi = acos(-1);
    keys_t *keys;
    char *filename;

    if(argc < 2)
    {
        filename = "testworld.txt";
    }
    else
    {
        filename = argv[1];
    }
    

    if(render_init() != 0)
    {
        return -1;
    }

    input_init();

    printf("Loading level %s\n", filename);
    world_load(filename);

    world = world_get_world();
    player = &world->player;

    // Print player sector position
    //printf("Player sector %i: ", player->sector);
    //sector_t *sect = &world->sectors[player->sector];
    //for(int i = 0; i < sect->num_vert; ++i)
    //{
    //    xy_t *vert = &world->vertices[sect->vertices[i]];
    //    printf("%4.2f %4.2f ", vert->x, vert->y);
    //}
    //printf("\n");

    while(done == 0)
    {
        input_update();
        keys = input_get();
        if(keys->q)
        {
            done = 1;
            continue;
        }
        
        world_move_player(keys);

        /** Update the screen */
        render_draw_world();
    }

    SDL_DestroyTexture(sprite.img);

    render_close();

    return 0;
}
