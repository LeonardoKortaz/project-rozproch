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
int mouse_x = 0, mouse_y = 0, mouse_click = 0;

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
    SDL_Window* window = SDL_CreateWindow("terraria2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, BLOCK_SIZE*WORLD_SIZE_X, BLOCK_SIZE*WORLD_SIZE_Y, SDL_WINDOW_SHOWN | !SDL_WINDOW_RESIZABLE);
    SDL_SetWindowResizable(window, SDL_FALSE); // TODO: make window not resizable (idk why this doesn't work)
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
            SDL_Rect rect = {players[i]->x, players[i]->y, BLOCK_SIZE, BLOCK_SIZE};
            SDL_QueryTexture(players[i]->texture, NULL, NULL, &rect.w, &rect.h);
            if(players[i]->facing == 0){
                SDL_RendererFlip flip = SDL_FLIP_HORIZONTAL;
                SDL_RenderCopyEx(renderer, players[i]->texture, NULL, &rect, 0, NULL, flip);
            } else {
                SDL_RendererFlip flip = SDL_FLIP_NONE;
                SDL_RenderCopyEx(renderer, players[i]->texture, NULL, &rect, 0, NULL, flip);
            }
        }
    }
}

void draw_world()
{
    for (int y = 0; y < WORLD_SIZE_Y; ++y)
    {
        for (int x = 0; x < WORLD_SIZE_X; ++x)
        {
            SDL_Rect rect = {x*BLOCK_SIZE, y*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE};
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
            case SKY:
                SDL_SetRenderDrawColor(renderer, SKY_COLOR);
                SDL_RenderFillRect(renderer, &rect);
                break;
            case GRASS:
                SDL_SetRenderDrawColor(renderer, GRASS_COLOR);
                SDL_RenderFillRect(renderer, &rect);
                break;
            case DIRT:
                SDL_SetRenderDrawColor(renderer, DIRT_COLOR);
                SDL_RenderFillRect(renderer, &rect);
                break;
            case STONE:
                SDL_SetRenderDrawColor(renderer, STONE_COLOR);
                SDL_RenderFillRect(renderer, &rect);
                break;

            }
        }
    }
    
    if(SDL_WINDOW_MOUSE_FOCUS){
        SDL_Rect rect = {mouse_x*WORLD_SIZE_X/2, mouse_y*WORLD_SIZE_Y/2, BLOCK_SIZE, BLOCK_SIZE};
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &rect);
    } else {
        SDL_Rect rect = {mouse_x*WORLD_SIZE_X/2+1, mouse_y*WORLD_SIZE_Y/2+1, 0, 0};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderDrawRect(renderer, &rect); // this shit doesnt work ( TODO: make rectangle disappear when mouse is out of range of building or not focused on the window )
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
        players[id]->facing = update->facing;
        // TODO: check timestamps
    }
    else
    {
        players[id] = malloc(sizeof (struct player_t));
        players[id]->x = 100;
        players[id]->y = 100;
        SDL_Surface* surface = SDL_LoadBMP("player.bmp"); // loads player texture (TODO: different texture depending on id)
        players[id]->texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
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
            case SDLK_SPACE:
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
            }
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case SDLK_w:
                input = input & ~UP;
                break ;
            case SDLK_SPACE:
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

    mouse_click = SDL_GetMouseState(&mouse_x, &mouse_y);
    mouse_x = (int)(mouse_x*2/WORLD_SIZE_X);
    mouse_y = (int)(mouse_y*2/WORLD_SIZE_Y); //snapping mouse coords to block coords

    
    switch(mouse_click){
        case 1:
            input = input | PLACE;
            break;
        case 4:
            input = input | BREAK;
            break;
    }

    struct datagram_t datagram;
    datagram.type = INPUT;
    datagram.data.input.id = id;
    datagram.data.input.timestamp = timer;
    datagram.data.input.input = input;
    datagram.data.input.mouse_update.pos_x = mouse_x;
    datagram.data.input.mouse_update.pos_y = mouse_y;
    datagram.data.input.mouse_update.input = mouse_click;

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
