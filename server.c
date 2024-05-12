#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "world.h"
#include "communication.h"
#include "player.h"

#define BUF_SIZE 500

int sfd;
struct player_t* players[MAX_PLAYERS];
char player_inputs[MAX_PLAYERS];
struct sockaddr_storage* peers_addr[MAX_PLAYERS];
socklen_t peers_addr_len[MAX_PLAYERS];
int mouse_x[MAX_PLAYERS], mouse_y[MAX_PLAYERS], mouse_click[MAX_PLAYERS];

// returns port
int check_arguments(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = strtol(argv[1], NULL, 10);
    if (port > 1024 && port < 65536)
    {
        return port;
    }
    fprintf(stderr, "Invalid port\n");
    exit(EXIT_FAILURE);
}

int create_server(int port)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1)
    {
        fprintf(stderr, "failed to create socket\n");
        exit(EXIT_FAILURE);
    }
    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        fprintf(stderr, "failed to bind socket\n");
        exit(EXIT_FAILURE);
    }
    return sfd;
}

int find_id()
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (peers_addr[i] == NULL)
        {
            return i;
        }
    }
    return -1;
}

void handle_join(struct sockaddr_storage* peer_addr, socklen_t peer_addr_len)
{
    struct datagram_t datagram;
    datagram.type = JOIN_APPROVAL;
    int id = find_id();
    datagram.data.approval.id = id;
    if (sendto(sfd, &datagram, sizeof(struct datagram_t), 0, (struct sockaddr*)peer_addr, peer_addr_len) != sizeof(struct datagram_t))
    {
        perror("failed");
        exit(1);
    }
    peers_addr[id] = malloc(sizeof(struct sockaddr_storage));
    memcpy(peers_addr[id], peer_addr, sizeof *peer_addr);
    peers_addr_len[id] = peer_addr_len;
    players[id] = calloc(1, sizeof(struct player_t));
    players[id]->x = 0;
    players[id]->y = 0;
    players[id]->facing = 1;
}

void handle_input(struct datagram_t datagram)
{
    unsigned int id = datagram.data.input.id;
    char input = datagram.data.input.input;
    player_inputs[id] = input;
    mouse_x[id] = datagram.data.input.mouse_update.pos_x;
    mouse_y[id] = datagram.data.input.mouse_update.pos_y;
    mouse_click[id] = datagram.data.input.mouse_update.input;
}

void handle_network()
{
    int s;
    ssize_t nread = 0;
    struct sockaddr_storage peer_addr;
    struct datagram_t request;
    socklen_t peer_addr_len = sizeof peer_addr;

    while (nread != -1)
    {
        nread = recvfrom(sfd, &request, sizeof request, MSG_DONTWAIT, (struct sockaddr*)&peer_addr, &peer_addr_len);
        if (nread == -1)
        {
            return;
        }

        char host[NI_MAXHOST], service[NI_MAXSERV];

        s = getnameinfo((struct sockaddr*)&peer_addr, peer_addr_len, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
        if (s == 0)
        {
            switch (request.type)
            {
            case JOIN_REQUEST:
                printf("Received %zd bytes from %s:%s\n", nread, host, service);
                handle_join(&peer_addr, peer_addr_len);
                break;
            case INPUT:
                handle_input(request);
                break;
                /*            case PLAYER_UPDATE:
                            break;
                        case JOIN_REQUEST:
                            break;
                        case JOIN_REFUSE:
                            break;*/
            }
        }
        else
        {
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
        }
    }
}

void update_players()
{
    for (int j = 0; j < MAX_PLAYERS; j++)
    {
        if (peers_addr[j] != NULL)
        {
            for (int i = 0; i < MAX_PLAYERS; ++i)
            {
                if (players[i])
                {
                    struct datagram_t datagram;
                    datagram.type = PLAYER_UPDATE;
                    datagram.data.update.id = i;
                    datagram.data.update.timestamp = 0;
                    datagram.data.update.pos_x = players[i]->x;
                    datagram.data.update.pos_y = players[i]->y;
                    datagram.data.update.facing = players[i]->facing;

                    if (sendto(sfd, &datagram, sizeof(struct datagram_t), 0, (struct sockaddr*)peers_addr[j], peers_addr_len[j]) != sizeof(struct datagram_t))
                    {
                        perror("failed to send");
                    }
                }
            }
            struct datagram_t datagram;
            datagram.type = WORLD_UPDATE;
            memcpy(datagram.data.world_update.world, world, sizeof world);
            if (sendto(sfd, &datagram, sizeof(struct datagram_t), 0, (struct sockaddr*)peers_addr[j], peers_addr_len[j]) != sizeof(struct datagram_t))
                perror("failed to send");
        }
    }
}

float acceleration = 0;

void game_tick(float delta)
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (players[i])
        {
            int on_ground = 0;
            char input = player_inputs[i];
            float speed = 100 * delta;
            if(world[(int)players[i]->y/BLOCK_SIZE + 1][(int)players[i]->x/BLOCK_SIZE] != SKY || world[(int)players[i]->y/BLOCK_SIZE + 1][((int)players[i]->x + 14)/BLOCK_SIZE] != SKY){
                on_ground = 1; // check if on ground
            }
            if(on_ground == 1 || (float)players[i]->y == WORLD_SIZE_Y*BLOCK_SIZE){
                acceleration = 0; // stop falling while on ground
            }
            if ((input & UP) && on_ground == 1){
                acceleration = 350 * delta; // jump
            }
            if ((input & LEFT) && (float)players[i]->x/BLOCK_SIZE > 0 && world[(int)players[i]->y/BLOCK_SIZE][(int)players[i]->x/BLOCK_SIZE] == SKY){
                players[i]->x -= speed; // go left
                players[i]->facing = 0; 
            }
            if ((input & RIGHT) && (int)players[i]->x/BLOCK_SIZE < WORLD_SIZE_X - 1 && world[(int)players[i]->y/BLOCK_SIZE][((int)players[i]->x + 15)/BLOCK_SIZE] == SKY){
                players[i]->x += speed; // go right
                players[i]->facing = 1;
            }
            if (input & PLACE)
            {   
                int occupied = 0;
                if(world[mouse_y[i]][mouse_x[i]] == SKY){
                    if(world[mouse_y[i]-1][mouse_x[i]] != SKY || world[mouse_y[i]+1][mouse_x[i]] != SKY || world[mouse_y[i]][mouse_x[i]-1] != SKY || world[mouse_y[i]][mouse_x[i]+1] != SKY){
                        for (int j = 0; j < MAX_PLAYERS; ++j){ 
                            if(players[j]) {
                                if(((int)players[j]->x/BLOCK_SIZE == mouse_x[j] || (((int)players[j]->x + 15)/BLOCK_SIZE) == mouse_x[j]) && (int)players[j]->y/BLOCK_SIZE == mouse_y[j]) occupied = 1;
                            }
                        }
                        if (occupied == 0) world[mouse_y[i]][mouse_x[i]] = STONE; // place block ( TODO: choose a block to place ) ( TODO: limit range of placement )
                    }
                }
            }
            if (input & BREAK)
            {
                if(world[mouse_y[i]][mouse_x[i]] != SKY){
                    world[mouse_y[i]][mouse_x[i]] = SKY; // break block ( TODO: limit range of breaking )
                }
            }
            if((world[(int)(players[i]->y + 8)/BLOCK_SIZE - 1][(int)players[i]->x/BLOCK_SIZE] != SKY || world[(int)(players[i]->y + 8)/BLOCK_SIZE - 1][((int)players[i]->x + 13)/BLOCK_SIZE] != SKY) && acceleration > 0){
                acceleration = 0; // dont jump through a block above you
            }
            players[i]->y -= acceleration; // power of GRAVITY
            if (acceleration > -300 * delta && world[(int)players[i]->y/BLOCK_SIZE + 1][(int)players[i]->x/BLOCK_SIZE] == SKY){
                acceleration -= 10 * delta; // gravity
            }
        }
    }
}

void loop()
{
    float dt = 0.1;
    
    for(int i = 0; i < WORLD_SIZE_X; i++){ // Creating basic world with ground
        for (int j = 0; j < WORLD_SIZE_Y-2; j++){
            world[j][i] = SKY;
        }

        world[WORLD_SIZE_Y-1][i] = DIRT;
        world[WORLD_SIZE_Y-2][i] = GRASS;
    }

    world[WORLD_SIZE_Y-3][WORLD_SIZE_X-5] = GRASS;

    for (;;)
    {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        handle_network();
        game_tick(dt);
        update_players();
        usleep(10 * 1000);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        dt = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    }
}

int main(int argc, char* argv[])
{
    int port = check_arguments(argc, argv);
    sfd = create_server(port);

    loop();

}
