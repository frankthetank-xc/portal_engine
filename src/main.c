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
        xy_t a1 = {0, 0}, a2 = {10, 10}, b1 = {0, 10}, b2 = {10, 0};
        printf("Intersection: %i", lines_intersect(&a1, &a2, &b1, &b2));

        printf("Please specify a level to play\n");
        return -1;
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

        if(keys->e)
        {
            input_toggle_mouselook();
            keys->e = 0;
        }
        if(keys->f)
        {
            toggle_debug();
            keys->f = 0;
        }
        
        world_move_player(keys);

        /** Update the screen */
        render_draw_world();
    }

    SDL_DestroyTexture(sprite.img);

    render_close();

    return 0;
}
