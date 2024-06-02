#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "communication.h"
#include "player.h"
#include "world.h"

#define BUF_SIZE 500

int sfd;
SDL_Renderer *renderer;
unsigned int id;
struct player_t* players[MAX_PLAYERS];
inventory_t inventory = {0, 0, 0};
char input;
char finish;
unsigned long long timer;
int mouse_x = 0, mouse_y = 0, mouse_click = 0;
unsigned int timeout;
enum blocks selected;
TTF_Font* font;
int update_required = 1;


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

SDL_Texture* textures[3];

void load_textures(){
    SDL_Surface* surface = SDL_LoadBMP("textures/grass_texture.bmp");
    textures[0] = SDL_CreateTextureFromSurface(renderer, surface);
    surface = SDL_LoadBMP("textures/dirt_texture.bmp");
    textures[1] = SDL_CreateTextureFromSurface(renderer, surface);
    surface = SDL_LoadBMP("textures/stone_texture.bmp");
    textures[2] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    font = TTF_OpenFont("textures/PixeloidMono-d94EV.ttf", 8);
    if(font == NULL){
        printf("TTF_OpenFont error: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }
}

void init_SDL()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    SDL_Window* window = SDL_CreateWindow("terraria2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, BLOCK_SIZE*WORLD_SIZE_X, BLOCK_SIZE*WORLD_SIZE_Y, SDL_WINDOW_SHOWN | !SDL_WINDOW_RESIZABLE);
    SDL_SetWindowResizable(window, SDL_FALSE); // TODO: make window not resizable (idk why this doesn't work)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    load_textures();
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
    selected = GRASS;
    return 1;
}

int num_len (int n) {
    if (n < 10) return 1;
    return 2;
}

SDL_Surface* blocks_txt[3];
SDL_Texture* number_txt[3];
SDL_Rect blocks_rect[3];
SDL_Color white = {255, 255, 255};

void draw_ui(){
    SDL_Rect rect = {0, 0, BLOCK_SIZE*5, BLOCK_SIZE*2}; // inventory
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_Rect rect0 = {8, 8, BLOCK_SIZE, BLOCK_SIZE};
    SDL_RenderCopy(renderer, textures[0], NULL, &rect0);
    SDL_Rect rect1 = {32, 8, BLOCK_SIZE, BLOCK_SIZE};
    SDL_RenderCopy(renderer, textures[1], NULL, &rect1);
    SDL_Rect rect2 = {56, 8, BLOCK_SIZE, BLOCK_SIZE};
    SDL_RenderCopy(renderer, textures[2], NULL, &rect2);
    
    int block_x;

    switch (selected){
        case (GRASS):
            block_x = 0;
            break;
        case (DIRT):
            block_x = 24;
            break;
        case (STONE):
            block_x = 48;
            break;
    }
    SDL_Rect rect3 = {8 + block_x, 8, BLOCK_SIZE, BLOCK_SIZE}; // selected block
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &rect3);

    if(update_required == 1){ // update number of blocks owned
        char* n = malloc(10);
        for(int i = 0; i < 3; i++){
            sprintf(n, "%d", inventory[i+4]);
            // printf("inventory %d %d %d %d %d %d\n", inventory[1], inventory[2], inventory[3], inventory[4], inventory[5], inventory[6]);
            blocks_txt[i] = TTF_RenderText_Solid(font, n, white);
            if (blocks_txt[i] == NULL){
                printf("TTF_RenderText_Solid error: %s\n", TTF_GetError());
                exit(EXIT_FAILURE);
            }
            number_txt[i] = SDL_CreateTextureFromSurface(renderer, blocks_txt[i]);
            blocks_rect[i].x = 9+i*24;
            blocks_rect[i].y = 15;
            blocks_rect[i].w = num_len(inventory[i+4])*8;
            blocks_rect[i].h = 10;
            update_required = 0;
        }
        free(n);
    }

    for (int i = 0; i < 3; i++){ // draw number of blocks owned
        SDL_RenderCopy(renderer, number_txt[i], NULL, &blocks_rect[i]);
    }
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
    draw_ui();
}

void draw_world()
{
    for (int y = 0; y < WORLD_SIZE_Y; ++y)
    {
        for (int x = 0; x < WORLD_SIZE_X; ++x)
        {
            if((y == 0 && x == 0) || (y == 0 && x == 1) || (y == 0 && x == 2) || (y == 0) && (x == 3) || (y == 0 && x == 4) || 
                (y == 1 && x == 0) || (y == 1 && x == 1) || (y == 1 && x == 2) || (y == 1 && x == 3) || (y == 1 && x == 4)) continue;

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
                SDL_RenderCopy(renderer, textures[0], NULL, &rect);
                break;
            case DIRT:
                SDL_RenderCopy(renderer, textures[1], NULL, &rect);
                break;
            case STONE:
                SDL_RenderCopy(renderer, textures[2], NULL, &rect);
                break;
            
            }
            
        }
    }
    
    if(SDL_WINDOW_MOUSE_FOCUS){
        SDL_Rect rect = {mouse_x * BLOCK_SIZE, mouse_y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE};
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
        SDL_Surface* surface;
        switch (id%5){
            case 0:
                surface = SDL_LoadBMP("textures/player.bmp");
                break;
            case 1:
                surface = SDL_LoadBMP("textures/player1.bmp");
                break;
            case 2:
                surface = SDL_LoadBMP("textures/player2.bmp");
                break;
            case 3:
                surface = SDL_LoadBMP("textures/player3.bmp");
                break;
            case 4:
                surface = SDL_LoadBMP("textures/player5.bmp");
                break;
        }
        players[id]->texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
}

void update_world(struct datagram_t* datagram)
{
    memcpy(world, datagram->data.world_update.world, sizeof world);
}

void update_inventory(struct datagram_t* datagram)
{
    memcpy(inventory, datagram->data.inventory_update.inventory, sizeof inventory);
    update_required = 1;
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
            timeout = TIMEOUT;
            switch (datagram.type)
            {
            case PLAYER_UPDATE:
                update_player(&datagram);
                break;
            case WORLD_UPDATE:
                update_world(&datagram);
                break;
            case INVENTORY_UPDATE:
                update_inventory(&datagram);
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
            case SDLK_1:
                selected = GRASS;
                break ;
            case SDLK_2:
                selected = DIRT;
                break ;
            case SDLK_3:
                selected = STONE;
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
    mouse_x = (int)(mouse_x / (double)BLOCK_SIZE);
    mouse_y = (int)(mouse_y / (double)BLOCK_SIZE); //snapping mouse coords to block coords


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
    datagram.data.input.selected_block = selected;

    if (send(sfd, &datagram, sizeof(struct datagram_t), 0) != sizeof(struct datagram_t))
        perror("failed to send input");
    timer++;
    //printf("%d\n", timer);
}

void loop()
{
    timeout = TIMEOUT;
    while (!finish)
    {
        listen_server();
        handle_keys();
        draw();
        timeout--;
        if (timeout == 0)
        {
            printf("connection lost\n");
            return ;
        }
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
