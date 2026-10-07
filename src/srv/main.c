#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "common.h"
#include "file.h"
#include "parse.h"
#include "srvpoll.h"

clientstate_t clientStates[MAX_CLIENTS] = {0};

static int g_dbfd = -1;
static struct dbheader_t *g_dbhdr = NULL;
static struct employee_t *g_employees = NULL;

void sigint_handler(int signum) {
    (void)signum;
    printf("\n[Server] Shutting down... Saving database to disk.\n");
    if (g_dbfd != -1 && g_dbhdr != NULL) {
        if (output_file(g_dbfd, g_dbhdr, g_employees) == STATUS_ERROR) {
            printf("[Server] Failed to write database on exit.\n");
        } else {
            printf("[Server] Database saved successfully.\n");
        }
        close(g_dbfd);
    }
    if (g_employees != NULL) {
        free(g_employees);
    }
    if (g_dbhdr != NULL) {
        free(g_dbhdr);
    }
    exit(EXIT_SUCCESS);
}

void print_usage(char *argv[]) {
    printf("Usage: %s -f <database file> -p <port> [options]\n", argv[0]);
    printf("Options:\n");
    printf("\t -f <file>    (Required) Path to database file\n");
    printf("\t -p <port>    (Required) Port to listen to\n");
    printf("\t -n           Create new database file\n");
}

void poll_loop(unsigned short port, struct dbheader_t *dbhdr, struct employee_t **employeeptr, int dbfd) {
    int listen_fd, conn_fd, freeSlot;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;
    int opt = 1;

    init_clients(clientStates);

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[Server] socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("[Server] setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("[Server] bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 10) == -1) {
        perror("[Server] listen");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d\n", port);

    while (1) {
        memset(fds, 0, sizeof(fds));
        fds[0].fd = listen_fd;
        fds[0].events = POLLIN;

        int ii = 1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clientStates[i].fd != -1) {
                fds[ii].fd = clientStates[i].fd;
                fds[ii].events = POLLIN;
                ii++;
            }
        }
        nfds = ii;

        int n_events = poll(fds, nfds, -1);
        if (n_events == -1) {
            perror("[Server] poll");
            exit(EXIT_FAILURE);
        }

        // Check for new connections
        if (fds[0].revents & POLLIN) {
            n_events--;
            if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
                perror("[Server] accept");
            } else {
                printf("[Server] New connection from %s:%d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                freeSlot = find_free_slot(clientStates);
                if (freeSlot == -1) {
                    printf("[Server] Server full: closing new connection\n");
                    close(conn_fd);
                } else {
                    clientStates[freeSlot].fd = conn_fd;
                    clientStates[freeSlot].state = STATE_HELLO;
                    memset(clientStates[freeSlot].buffer, 0, sizeof(clientStates[freeSlot].buffer));
                    printf("[Server] Slot %d allocated for fd %d\n", freeSlot, clientStates[freeSlot].fd);
                }
            }
        }

        // Check each client for read/write activity
        for (int i = 1; i < nfds && n_events > 0; i++) { // Start from 1 to skip the listen_fd
            if (fds[i].revents & POLLIN) {
                n_events--;

                int fd = fds[i].fd;
                int slot = find_slot_by_fd(clientStates, fd);
                if (slot == -1) {
                    continue;
                }

                ssize_t bytes_read = read(fd, clientStates[slot].buffer, sizeof(clientStates[slot].buffer) - 1);
                if (bytes_read <= 0) {
                    close(fd);
                    clientStates[slot].fd = -1;
                    clientStates[slot].state = STATE_DISCONNECTED;
                    memset(clientStates[slot].buffer, 0, sizeof(clientStates[slot].buffer));
                    printf("[Server] Client on fd %d disconnected\n", fd);
                } else {
                    clientStates[slot].buffer[bytes_read] = '\0';
                    handle_client_fsm(dbhdr, employeeptr, &clientStates[slot], dbfd);
                    g_employees = *employeeptr;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    char *filepath = NULL;
    char *portarg = NULL;
    unsigned short port = 0;
    bool newfile = false;
    int c;

    int dbfd = -1;
    struct dbheader_t *dbhdr = NULL;
    struct employee_t *employees = NULL;

    while ((c = getopt(argc, argv, "nf:p:")) != -1) {
        switch (c) {
            case 'n':
                newfile = true;
                break;
            case 'f':
                filepath = optarg;
                break;
            case 'p':
                portarg = optarg;
                port = (unsigned short)atoi(portarg);
                break;
            case '?':
                print_usage(argv);
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;      
        }
    }

    if (filepath == NULL) {
        printf("[Server] Filepath is a required argument\n");
        print_usage(argv);
        return EXIT_FAILURE;
    }

    if (port == 0) {
        printf("[Server] Bad port: %s\n", portarg ? portarg : "NULL");
        print_usage(argv);
        return EXIT_FAILURE;
    }

    if (newfile) {
        dbfd = create_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            printf("[Server] Failed to create database file: %s\n", filepath);
            return EXIT_FAILURE;
        }

        if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            printf("[Server] Failed to create database header\n");
            close(dbfd);
            return EXIT_FAILURE;
        }
    } else {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            printf("[Server] Failed to open database file: %s\n", filepath);
            return EXIT_FAILURE;
        }

        if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            printf("[Server] Failed to validate database header\n");
            close(dbfd);
            return EXIT_FAILURE;
        }

        if (read_employees(dbfd, dbhdr, &employees) == STATUS_ERROR) {
            printf("[Server] Failed to read employees\n");
            close(dbfd);
            free(dbhdr);
            return EXIT_FAILURE;
        }
    }

    g_dbfd = dbfd;
    g_dbhdr = dbhdr;
    g_employees = employees;
    signal(SIGINT, sigint_handler);
    
    poll_loop(port, dbhdr, &employees, dbfd);

    return EXIT_SUCCESS;
}
