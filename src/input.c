/**
 * File to handle user inputs
 */

/* ***********************************
 * Includes
 * ***********************************/
// My header
#include "input.h"

// Global Headers
#include <stdio.h>
#include <string.h>
#include <SDL.h>

// Project headers
#include "common.h"

/* ***********************************
 * Private Definitions
 * ***********************************/

/* ***********************************
 * Private Typedefs
 * ***********************************/

/* ***********************************
 * Static variables
 * ***********************************/

/** Global window object */
static keys_t _keys;

/* ***********************************
 * Static function prototypes
 * ***********************************/

/* ***********************************
 * Static function implementation
 * ***********************************/

/* ***********************************
 * Public function implementation
 * ***********************************/

void input_init()
{
    memset(&_keys, 0, sizeof(_keys));

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_ENABLE);
}

/**
 * Update user input status. Should be run every frame.
 */ 
void input_update(void)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        /** Get key inputs */
        switch(event.type)
        {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                switch(event.key.keysym.sym)
                {
                    case 'w': _keys.w = (event.type == SDL_KEYDOWN); break;
                    case 'a': _keys.a = (event.type == SDL_KEYDOWN); break;
                    case 's': _keys.s = (event.type == SDL_KEYDOWN); break;
                    case 'd': _keys.d = (event.type == SDL_KEYDOWN); break;
                    case SDLK_RIGHT: _keys.right = (event.type == SDL_KEYDOWN); break;
                    case SDLK_LEFT: _keys.left = (event.type == SDL_KEYDOWN); break;
                    case SDLK_UP: _keys.up = (event.type == SDL_KEYDOWN); break;
                    case SDLK_DOWN: _keys.down = (event.type == SDL_KEYDOWN); break;
                    case ' ': _keys.space = (event.type == SDL_KEYDOWN); break;
                    case 'q': _keys.q = (event.type == SDL_KEYDOWN); break;
                    default: break;
                }
                break;
            case SDL_QUIT: 
                _keys.q = 1; break;
            default:
                break;
        }
    }
}

/**
 * Return a handle to the keys_t struct holding key info
 */
keys_t *input_get(void)
{
    return &_keys;
}

void mouse_get_input(int *x, int *y)
{
    SDL_GetRelativeMouseState(x, y);
}