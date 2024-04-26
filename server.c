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
}

void handle_input(struct datagram_t datagram)
{
    unsigned int id = datagram.data.input.id;
    char input = datagram.data.input.input;
    player_inputs[id] = input;
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

void game_tick(float delta)
{
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (players[i])
        {
            char input = player_inputs[i];
            float speed = 100 * delta;
            if (input & UP)
                players[i]->y -= speed;
            if (input & DOWN)
                players[i]->y += speed;
            if (input & LEFT)
                players[i]->x -= speed;
            if (input & RIGHT)
                players[i]->x += speed;
            if (input & PLACE)
            {
                world[(int)players[i]->y/32][(int)players[i]->x/32] = SOLID;
                printf("%d %d %d\n", (int)players[i]->y, (int)players[i]->x, SOLID);
            }
            if (input & BREAK)
            {
                world[(int)players[i]->y/32][(int)players[i]->x/32] = EMPTY;
                printf("%d %d %d\n", (int)players[i]->y, (int)players[i]->x, EMPTY);
            }
        }
    }
}

void loop()
{
    float dt = 0.1;
    world[0][0] = SOLID;
    world[1][0] = WHATEVER;

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
