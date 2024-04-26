#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "communication.h"
#include "player.h"
#include "world.h"

#define BUF_SIZE 500

int sfd;
SDL_Renderer *renderer;
unsigned int id;
struct player_t* players[MAX_PLAYERS];
char input;
char finish;
unsigned int timer;

int create_socket(int argc, char* argv[])
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s host port msg...\n", argv[0]);
        exit(EXIT_FAILURE);
    }
     /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */

    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return 1;
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        printf("try %d, %d, %d\n", rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) break; /* Success */
        printf("failed to connect()");
        close(sfd);
    }
    printf("connect\n");

    freeaddrinfo(result); /* No longer needed */

    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        return 1;
    }
    return 0;
}

void init_SDL()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("terraria2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1000, 800, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

int connect_to_server(int argc, char* argv[])
{
    if (create_socket(argc, argv) != 0)
        exit(EXIT_FAILURE);

    for (int i = 0; i < 5; i++)
    {
        struct datagram_t datagram;
        struct datagram_t response;
        int n;
        memset(&datagram, 0, sizeof datagram);
        datagram.type = JOIN_REQUEST;
        if (send(sfd, &datagram, sizeof(struct datagram_t), 0) != sizeof(struct datagram_t))
        {
            perror("failed to send");
        }
        usleep(500*1000);
        n = recv(sfd, &response, sizeof(struct datagram_t), MSG_DONTWAIT);
        printf("received %d\n", n);
        if (n == sizeof(struct datagram_t))
        {
            if (response.type == JOIN_APPROVAL)
            {
                id = response.data.approval.id;
                printf("connected as %d\n", id);
                return 0;
            }
            if (response.type == JOIN_REFUSE)
            {
                printf("server refused to join\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return 1;
}

void draw_players()
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (players[i])
        {
            SDL_Rect rect = {players[i]->x, players[i]->y, 10, 10};
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
}

void draw_world()
{
    for (int y = 0; y < WORLD_SIZE; ++y)
    {
        for (int x = 0; x < WORLD_SIZE; ++x)
        {
            SDL_Rect rect = {x*32, y*32, 32, 32};
            switch (world[y][x])
            {
            case EMPTY:
                break;
            case SOLID:
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &rect);
                break;
            case WHATEVER:
                SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
                SDL_RenderFillRect(renderer, &rect);
                break;
            }
        }
    }
}

void draw()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    draw_world();
    draw_players();
    SDL_RenderPresent(renderer);
}

void update_player(struct datagram_t* datagram)
{
    struct player_update_t* update = &datagram->data.update;
    unsigned int id = update->id;
    if (players[id])
    {
        players[id]->x = update->pos_x;
        players[id]->y = update->pos_y;
        // TODO: check timestamps
    }
    else
    {
        players[id] = malloc(sizeof (struct player_t));
        players[id]->x = 100;
        players[id]->y = 100;
    }
}

void update_world(struct datagram_t* datagram)
{
    memcpy(world, datagram->data.world_update.world, sizeof world);
}

void listen_server()
{
    struct datagram_t datagram;
    int n = 0;

    while (n != -1)
    {
        n = recv(sfd, &datagram, sizeof(struct datagram_t), MSG_DONTWAIT);
        //printf("received %d\n", n);
        if (n == sizeof(struct datagram_t))
        {
            switch (datagram.type)
            {
            case PLAYER_UPDATE:
                update_player(&datagram);
                break;
            case WORLD_UPDATE:
                update_world(&datagram);
                break;
            }
        }
    }
}

void handle_keys()
{
    SDL_Event event;
    input = input & ~PLACE;
    input = input & ~BREAK;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDLK_q:
                finish = 1;
                break ;
            case SDLK_w:
                input = input | UP;
                break ;
            case SDLK_s:
                input = input | DOWN;
                break ;
            case SDLK_a:
                input = input | LEFT;
                break ;
            case SDLK_d:
                input = input | RIGHT;
                break ;
            case SDLK_e:
                input = input | PLACE;
                break ;
            case SDLK_r:
                input = input | BREAK;
                break ;
            }
            break ;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case SDLK_w:
                input = input & ~UP;
                break ;
            case SDLK_s:
                input = input & ~DOWN;
                break ;
            case SDLK_a:
                input = input & ~LEFT;
                break ;
            case SDLK_d:
                input = input & ~RIGHT;
                break ;
            }
        }
    }
    if (input & PLACE)
        printf("place\n");


    struct datagram_t datagram;
    datagram.type = INPUT;
    datagram.data.input.id = id;
    datagram.data.input.timestamp = timer;
    datagram.data.input.input = input;

    if (send(sfd, &datagram, sizeof(struct datagram_t), 0) != sizeof(struct datagram_t))
        perror("failed to send input");
    timer++;
    printf("%d\n", timer);
}

void loop()
{
    while (!finish)
    {
        listen_server();
        handle_keys();
        draw();
        SDL_Delay(10);
    }
}

int main(int argc, char* argv[])
{
    printf("client lol\n");
    init_SDL();
    if (connect_to_server(argc, argv) != 0)
    {
        printf("failed to connect\n");
        exit(EXIT_FAILURE);
    }
    loop();

    exit(EXIT_SUCCESS);
}
