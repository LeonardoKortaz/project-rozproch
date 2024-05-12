//
// Created by kamil on 04.04.24.
//

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "world.h"
enum data_t
{
    JOIN_APPROVAL,
    INPUT,
    PLAYER_UPDATE,
    JOIN_REQUEST,
    JOIN_REFUSE,
    WORLD_UPDATE
};

enum input_mask
{
    LEFT = 1,
    RIGHT = 2,
    UP = 4,
    DOWN = 8,
    PLACE = 16,
    BREAK = 32
};

struct input_data_t
{
    unsigned int id;
    unsigned int timestamp;
    char input;
    struct mouse_update_t
    {
        int pos_x;
        int pos_y;
        int input;
    } mouse_update;
};



struct player_update_t
{
    unsigned int id;
    unsigned int timestamp;
    int pos_x;
    int pos_y;
    int facing;
};

struct join_request_t
{};

struct join_approval_t
{
    unsigned int id;
};

enum refuse_reason
{
    GAME_FULL
};

struct join_refuse_t
{
    enum refuse_reason reason;
};

struct world_update_t
{
    enum blocks world[WORLD_SIZE_Y][WORLD_SIZE_X];
};

union datagram_data_t
{
    struct input_data_t input;
    struct player_update_t update;
    struct join_request_t request;
    struct join_approval_t approval;
    struct join_refuse_t refuse;
    struct world_update_t world_update;
};

struct datagram_t
{
    enum data_t type;
    union datagram_data_t data;
};

#endif // COMMUNICATION_H
