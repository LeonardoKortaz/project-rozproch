//
// Created by kamil on 04.04.24.
//

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

enum data_t
{
    JOIN_APPROVAL,
    INPUT,
    PLAYER_UPDATE,
    JOIN_REQUEST,
    JOIN_REFUSE
};

enum input_mask
{
    LEFT = 1,
    RIGHT = 2,
    UP = 4,
    DOWN = 8
};

struct input_data_t
{
    unsigned int id;
    unsigned int timestamp;
    char input;
};

struct player_update_t
{
    unsigned int id;
    unsigned int timestamp;
    int pos_x;
    int pos_y;
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

union datagram_data_t
{
    struct input_data_t input;
    struct player_update_t update;
    struct join_request_t request;
    struct join_approval_t approval;
    struct join_refuse_t refuse;
};

struct datagram_t
{
    enum data_t type;
    union datagram_data_t data;
};

#endif // COMMUNICATION_H
