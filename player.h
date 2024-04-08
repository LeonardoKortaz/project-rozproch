#ifndef PLAYER_H
#define PLAYER_H

#include "blocks.h"

struct player_t
{
    int x;
    int y;
    enum blocks selected;
};

#define MAX_PLAYERS 5

#endif