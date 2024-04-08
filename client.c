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

#define BUF_SIZE 500

int sfd;
SDL_Renderer *renderer;
unsigned int id;
struct player_t* players[MAX_PLAYERS];
int input_x;
int input_y;
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

    char connected = 0;
    while (!connected)
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
                connected = 1;
            }
            if (response.type == JOIN_REFUSE)
            {
                printf("server refused to join\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void draw()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (players[i])
        {
            SDL_Rect rect = {players[i]->x, players[i]->y, 10, 10};
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
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

void listen_server()
{
    struct datagram_t datagram;
    int n = 0;

    while (n != -1)
    {
        n = recv(sfd, &datagram, sizeof(struct datagram_t), MSG_DONTWAIT);
        printf("received %d\n", n);
        if (n == sizeof(struct datagram_t))
        {
            switch (datagram.type)
            {
            case PLAYER_UPDATE:
                update_player(&datagram);
                break;
            }
        }
    }
}

void handle_keys()
{
    SDL_Event event;
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
                input_y = -1;
                break ;
            case SDLK_s:
                input = input | DOWN;
                input_y = 1;
                break ;
            case SDLK_a:
                input = input | LEFT;
                input_x = -1;
                break ;
            case SDLK_d:
                input = input | RIGHT;
                input_x = 1;
                break ;
            }
            break ;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
            case SDLK_w:
                input = input & ~UP;
                if (input_y == -1)
                input_y = 0;
                break ;
            case SDLK_s:
                input = input & ~DOWN;
                if (input_y == 1)
                input_y = 0;
                break ;
            case SDLK_a:
                input = input & ~LEFT;
                if (input_x == -1)
                input_x = 0;
                break ;
            case SDLK_d:
                input = input & ~RIGHT;
                if (input_x == 1)
                input_x = 0;
                break ;
            }
        }
    }


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
    connect_to_server(argc, argv);
    loop();

    size_t len;
    ssize_t nread;
    char buf[BUF_SIZE];

    /* Send remaining command-line arguments as separate
       datagrams, and read responses from server */

    for (int j = 3; j < argc; j++)
    {
        //for (int foo = 0; foo < 1000000; foo++)
        //{
            len = strlen(argv[j]) + 1;
            /* +1 for terminating null byte */

            if (len > BUF_SIZE)
            {
                fprintf(stderr, "Ignoring long message in argument %d\n", j);
                continue;
            }

            if (write(sfd, argv[j], len) != len)
            {
                fprintf(stderr, "partial/failed write\n");
                exit(EXIT_FAILURE);
            }

            printf("sleep\n");
            sleep(1);
            printf("wakey\n");
            struct sockaddr_storage peer_addr;
            socklen_t peer_addr_len;
            nread = recvfrom(sfd, buf, BUF_SIZE, MSG_DONTWAIT, (struct sockaddr*)&peer_addr, &peer_addr_len);
            if (nread == -1)
            {
                perror("read");
                exit(EXIT_FAILURE);
            }

            printf("Received %zd bytes: %s\n", nread, buf);
            printf("waiting for response\n");
            nread = recvfrom(sfd, buf, BUF_SIZE, MSG_DONTWAIT, (struct sockaddr*)&peer_addr, &peer_addr_len);
            printf("stopped waiting\n");
            if (nread == -1)
            {
                perror("read foo");
//                exit(EXIT_FAILURE);
            }
            else
            printf("Received %zd bytes: %s\n", nread, buf);
        //}
    }

    exit(EXIT_SUCCESS);
}
