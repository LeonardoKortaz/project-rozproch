#ifndef PLAYER_H
#define PLAYER_H

#include "blocks.h"
#include <SDL2/SDL_render.h>

struct player_t
{
    float x;
    float y;
    enum blocks selected;
    SDL_Texture* texture;
    int facing;
    float acceleration;
    long long unsigned last_update;
};

#define MAX_PLAYERS 5

#endif