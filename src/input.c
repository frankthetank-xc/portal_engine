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

#define STICK_MAX (32767)

/* ***********************************
 * Private Typedefs
 * ***********************************/

/* ***********************************
 * Static variables
 * ***********************************/

/** Global window object */
static keys_t _keys;
SDL_GameController *gameController;

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
    gameController = NULL;

    input_set_mouselook(1);
}

void input_close()
{
    if(gameController) SDL_GameControllerClose(gameController);
}

/**
 * Update user input status. Should be run every frame.
 */ 
void input_update(void)
{
    SDL_Event event;
    if(gameController == NULL)
    {
        // See if we can open the came controller
        if(SDL_NumJoysticks() > 0)
        {
            for(int i = 0; i < SDL_NumJoysticks(); ++i)
            {
                if(SDL_IsGameController(i))
                {
                    // Try to open the controller
                    gameController = SDL_GameControllerOpen(i);
                    if(gameController) 
                    {
                        SDL_GameControllerEventState(SDL_ENABLE);
                        break;
                    }
                }
            }
            
        }
    }
    else
    {
        if(SDL_GameControllerGetAttached(gameController) != SDL_TRUE)
        {
            SDL_GameControllerClose(gameController);
            gameController = NULL;
        }
    }
    
    while(SDL_PollEvent(&event))
    {
        /** Get key inputs */
        switch(event.type)
        {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                switch(event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: 
                        _keys.w = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        _keys.s = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        _keys.left = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        _keys.right = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_B:
                        _keys.c = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                        _keys.shift = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_A:
                        _keys.space = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                    case SDL_CONTROLLER_BUTTON_START:
                        _keys.q = (event.type == SDL_CONTROLLERBUTTONDOWN); break;
                }
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                switch(event.key.keysym.sym)
                {
                    case 'w': _keys.w = (event.type == SDL_KEYDOWN); break;
                    case 'a': _keys.a = (event.type == SDL_KEYDOWN); break;
                    case 's': _keys.s = (event.type == SDL_KEYDOWN); break;
                    case 'd': _keys.d = (event.type == SDL_KEYDOWN); break;
                    case 'e': _keys.e = (event.type == SDL_KEYDOWN); break;
                    case 'f': _keys.f = (event.type == SDL_KEYDOWN); break;
                    case 'c': _keys.c = (event.type == SDL_KEYDOWN); break;
                    case SDLK_RIGHT: _keys.right = (event.type == SDL_KEYDOWN); break;
                    case SDLK_LEFT: _keys.left = (event.type == SDL_KEYDOWN); break;
                    case SDLK_UP: _keys.up = (event.type == SDL_KEYDOWN); break;
                    case SDLK_DOWN: _keys.down = (event.type == SDL_KEYDOWN); break;
                    case SDLK_RSHIFT:
                    case SDLK_LSHIFT: _keys.shift = (event.type == SDL_KEYDOWN); break;
                    case ' ': _keys.space = (event.type == SDL_KEYDOWN); break;
                    case 'q': _keys.q = (event.type == SDL_KEYDOWN); break;
                    default: break;
                }
                break;
            case SDL_QUIT: 
                _keys.q = 1; break;
                break;
            default:
                break;
        }
    }
}

void input_set_mouselook(int enable)
{
    int x,y;
    if(enable)
    {
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_ENABLE);
    }
    else
    {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetRelativeMouseMode(SDL_DISABLE);
        // Get mouse state to clear out buffer
        SDL_GetRelativeMouseState(&x,&y);
    }
}

void input_toggle_mouselook(void)
{
    input_set_mouselook(SDL_GetRelativeMouseMode() == SDL_ENABLE ? 0 : 1);
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
    if(SDL_GetRelativeMouseMode() == SDL_DISABLE) 
    {
        *x = 0; *y = 0;
    }
    else 
    {
        SDL_GetRelativeMouseState(x, y);
    }
}

void input_get_joystick(float *lx, float *ly, float *rx, float *ry)
{
    if(gameController)
    {
        if(lx) *lx = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX)  / (float)STICK_MAX;
        if(ly) *ly = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY)  / (float)STICK_MAX;
        if(rx) *rx = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTX) / (float)STICK_MAX;
        if(ry) *ry = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTY) / (float)STICK_MAX;
    }
    else
    {
        if(lx) *lx = 0;
        if(ly) *ly = 0;
        if(rx) *rx = 0;
        if(ry) *ry = 0;
    }
    
}