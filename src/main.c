/**
 * Playground program to mess around with 3D SDL rendering
 * 
 * 
 * Initial code is heavily based on Bisqwit's 3D renderer application
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
#include "util.h"

/* ***********************************
 * Private Defines
 * ***********************************/
#define MOVE_LEN (1)
#define TICK_SPAN ((int)(1000 / 60))

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
    int done = 0, moving;
    world_t *world;
    player_t *player;
    double pi = acos(-1);
    keys_t *keys;
    char *filename;
    int fullscreen = 0;
    uint32_t lastTick, curTick;

    if(argc < 2)
    {
        printf("Please specify a level to play\n");
        return -1;
    }
    else
    {
        filename = argv[1];
    }

    if(argc > 2)
    {
        fullscreen = 1;
    }

    if(render_init(fullscreen) != 0)
    {
        return -1;
    }

    input_init();

    printf("Loading level %s\n", filename);
    if(world_load(filename) != 0)
    {
        printf("Could not load world %s\n", filename);
        input_close();
        render_close();
        return -1;
    }



    world = world_get_world();
    player = &world->player;
    lastTick = SDL_GetTicks();

    while(done == 0)
    {
        input_update();
        keys = input_get();
        if(keys->q)
        {
            done = 1;
            continue;
        }

        if(keys->e)
        {
            input_toggle_mouselook();
            keys->e = 0;
        }
        if(keys->f)
        {
            //toggle_debug();
            // Toggle fullscreen mode
            fullscreen = (fullscreen) ? 0 : 1;
            render_set_fullscreen(fullscreen);
            keys->f = 0;
        }
        curTick = SDL_GetTicks();
        while(curTick - lastTick > TICK_SPAN)
        {
            lastTick += TICK_SPAN;
            world_move_player(keys);
        }

        /** Update the screen */
        render_draw_world();
    }
    printf("Exiting...\n");

    input_close();
    world_close();
    render_close();

    return 0;
}
