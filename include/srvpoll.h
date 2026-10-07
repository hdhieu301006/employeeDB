#ifndef SRVPOLL_H
#define SRVPOLL_H

#include "parse.h"
#include <poll.h>

#define MAX_CLIENTS 256
#define PORT 8080
#define BUFF_SIZE 4096

typedef enum {
    STATE_NEW,
    STATE_CONNECTED,
    STATE_DISCONNECTED,
    STATE_HELLO,
    STATE_MSG,
    STATE_GOODBYE,
} state_e;

// Structure to hold client state
typedef struct {
    int fd;
    state_e state;
    char buffer[BUFF_SIZE];
} clientstate_t;

void init_clients(clientstate_t* states);

int find_free_slot(clientstate_t* states);

int find_slot_by_fd(clientstate_t* states, int fd);

void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t **employees, clientstate_t *client, int dbfd);

void poll_loop(unsigned short port, struct dbheader_t *dbhdr, struct employee_t **employeeptr, int dbfd);

#endif
