#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "world.h"
#include "communication.h"
#include "player.h"

#define BUF_SIZE 500

int sfd;
struct player_t* players[MAX_PLAYERS];
char ips[MAX_PLAYERS][NI_MAXHOST+NI_MAXSERV+1];
struct sockaddr_storage* peers_addr[MAX_PLAYERS];
socklen_t peer_addr_len;

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
        if (strcmp(ips[i], "") == 0)
        {
            return i;
        }
    }
    return -1;
}

void handle_join(struct datagram_t request, struct sockaddr_storage* peer_addr, socklen_t peer_addr_len, char (*host)[NI_MAXHOST], char (*service)[NI_MAXSERV])
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
    peers_addr[id] = peer_addr;
    sprintf(ips[id], "%s:%s", *host, *service);
    players[id] = calloc(1, sizeof(struct player_t));
    players[id]->x = 0;
    players[id]->y = 0;
}

void handle_input(struct datagram_t datagram)
{
    unsigned int id = datagram.data.input.id;
    char input = datagram.data.input.input;
    if (id == 1)
    printf("input time %d from %d\n", datagram.data.input.timestamp, id);
    if (players[id])
    {
        if (input & UP)
            players[id]->y--;
        if (input & DOWN)
            players[id]->y++;
        if (input & LEFT)
            players[id]->x--;
        if (input & RIGHT)
            players[id]->x++;
    }
}

void handle_network()
{
    int s;
    ssize_t nread;
    struct sockaddr_storage peer_addr;
    struct datagram_t request;
    peer_addr_len = sizeof(peer_addr);


    nread = recvfrom(sfd, &request, sizeof request, 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
    if (nread == -1) return;

    char host[NI_MAXHOST], service[NI_MAXSERV];

    s = getnameinfo((struct sockaddr*)&peer_addr, peer_addr_len, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
    if (s == 0)
    {
        switch (request.type)
        {
        case JOIN_REQUEST:
            printf("Received %zd bytes from %s:%s\n", nread, host, service);
            handle_join(request, &peer_addr, peer_addr_len, &host, &service);
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

void loop()
{
    for (;;)
    {
        handle_network();

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
                        datagram.data.update.id = i; // = {i, 0, players[i].x, players[i].y};
                        datagram.data.update.timestamp = 0;
                        datagram.data.update.pos_x = players[i]->x;
                        datagram.data.update.pos_y = players[i]->y;

                        if (sendto(sfd, &datagram, sizeof(struct datagram_t), 0, (struct sockaddr*)peers_addr[j], peer_addr_len) != sizeof(struct datagram_t))
                        {
                            perror("failed to send");
                        }
                    }
                }
            }
        }


        //printf("%s\n", ips[id]);

        //if (sendto(sfd, buf, nread, 0, (struct sockaddr*)&peer_addr, peer_addr_len) != nread) fprintf(stderr, "Error sending response\n");
    }
}

int main(int argc, char* argv[])
{
    int port = check_arguments(argc, argv);
    sfd = create_server(port);


    /* Read datagrams and echo them back to sender */

    loop();

}
