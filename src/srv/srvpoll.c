#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <poll.h>

#include "common.h"
#include "parse.h"
#include "srvpoll.h"

void fsm_reply_hello(clientstate_t *client, dbproto_hdr_t *hdr) {
    hdr->type = htons(MSG_HELLO_RESP);
    hdr->len = htons(sizeof(dbproto_hello_resp));

    dbproto_hello_resp *hello = (dbproto_hello_resp *)&hdr[1];
    hello->proto = htons(PROTO_VER);
    
    write(client->fd, hdr, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_resp));
}

void fsm_reply_hello_err(clientstate_t *client, dbproto_hdr_t *hdr) {
    hdr->type = htons(MSG_ERROR);
    hdr->len = htons(0);

    write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_ack(clientstate_t *client, dbproto_hdr_t *hdr, uint16_t resp_type) {
    hdr->type = htons(resp_type);
    hdr->len = htons(0);

    write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_err(clientstate_t *client, dbproto_hdr_t *hdr) {
    hdr->type = htons(MSG_ERROR);
    hdr->len = htons(0);

    write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void send_employees(struct dbheader_t *dbhdr, struct employee_t **employeeptr, clientstate_t *client) {
    uint16_t count = dbhdr->count;
    uint32_t payload_len = count * sizeof(dbproto_employee_list_resp);

    dbproto_hdr_t resp_hdr;
    resp_hdr.type = htons(MSG_EMPLOYEE_LIST_RESP);
    resp_hdr.len = htons((uint16_t)payload_len);
    
    if (write(client->fd, &resp_hdr, sizeof(dbproto_hdr_t)) != sizeof(dbproto_hdr_t)) {
        perror("[Server] write list header");
        return;
    }

    if (count == 0) {
        return;
    }

    dbproto_employee_list_resp *resp_data = calloc(count, sizeof(dbproto_employee_list_resp));
    if (!resp_data) {
        perror("[Server] calloc list response");
        return;
    }

    struct employee_t *employees = *employeeptr;
    for (int i = 0; i < count; i++) {
        strncpy(resp_data[i].name, employees[i].name, sizeof(resp_data[i].name) - 1);
        strncpy(resp_data[i].address, employees[i].address, sizeof(resp_data[i].address) - 1);
        resp_data[i].hours = htonl(employees[i].hours);
    }

    write(client->fd, resp_data, payload_len);
    free(resp_data);
}

void handle_client_fsm(struct dbheader_t *dbhdr, struct employee_t **employeeptr, clientstate_t *client, int dbfd) {
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)client->buffer;

    hdr->type = ntohs(hdr->type);
    hdr->len = ntohs(hdr->len);

    if (client->state == STATE_HELLO) {
        if (hdr->type != MSG_HELLO_REQ || hdr->len != sizeof(dbproto_hello_req)) {
            printf("[Server] Didn't get valid MSG_HELLO in HELLO state (got type: %d, len: %d)\n", hdr->type, hdr->len);
            fsm_reply_hello_err(client, hdr);
            return;
        }

        dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
        if (ntohs(hello->proto) != PROTO_VER) {
            printf("[Server] Protocol mismatch: expected %d, got %d\n", PROTO_VER, ntohs(hello->proto));
            fsm_reply_hello_err(client, hdr);
            return;
        }

        fsm_reply_hello(client, hdr);
        client->state = STATE_MSG;
        return;
    }

    if (client->state == STATE_MSG) {
        if (hdr->type == MSG_EMPLOYEE_ADD_REQ) {
            dbproto_employee_add_req *employee_req = (dbproto_employee_add_req *)&hdr[1];
            employee_req->data[sizeof(employee_req->data) - 1] = '\0';

            printf("[Server] Adding employee: %s\n", employee_req->data);
            if (add_employee(dbhdr, employeeptr, employee_req->data) == STATUS_ERROR) {
                fsm_reply_err(client, hdr);
            } else {
                fsm_reply_ack(client, hdr, MSG_EMPLOYEE_ADD_RESP);
                output_file(dbfd, dbhdr, *employeeptr);
            }
            return;
        }

        if (hdr->type == MSG_EMPLOYEE_LIST_REQ) {
            printf("[Server] Client requested employee list\n");
            send_employees(dbhdr, employeeptr, client);
            return;
        }

        if (hdr->type == MSG_EMPLOYEE_UPDATE_REQ) {
            dbproto_employee_update_req *update_req = (dbproto_employee_update_req *)&hdr[1];
            update_req->data[sizeof(update_req->data) - 1] = '\0';

            printf("[Server] Updating employee: %s\n", update_req->data);
            if (update_employee(dbhdr, *employeeptr, update_req->data) == STATUS_ERROR) {
                fsm_reply_err(client, hdr);
            } else {
                fsm_reply_ack(client, hdr, MSG_EMPLOYEE_UPDATE_RESP);
                output_file(dbfd, dbhdr, *employeeptr);
            }
            return;
        }

        if (hdr->type == MSG_EMPLOYEE_DEL_REQ) {
            dbproto_employee_del_req *del_req = (dbproto_employee_del_req *)&hdr[1];
            del_req->name[sizeof(del_req->name) - 1] = '\0';

            printf("[Server] Deleting employee: %s\n", del_req->name);
            if (delete_employee(dbhdr, employeeptr, del_req->name) == STATUS_ERROR) {
                fsm_reply_err(client, hdr);
            } else {
                fsm_reply_ack(client, hdr, MSG_EMPLOYEE_DEL_RESP);
                output_file(dbfd, dbhdr, *employeeptr);
            }
            return;
        }

        printf("[Server] Unknown message type: %d\n", hdr->type);
        fsm_reply_err(client, hdr);
    }
}

void init_clients(clientstate_t *states) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        states[i].fd = -1;
        states[i].state = STATE_NEW;
        memset(states[i].buffer, 0, BUFF_SIZE);
    }
}

int find_free_slot(clientstate_t *states) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (states[i].fd == -1) {
            return i;
        }
    }
    return -1;
}

int find_slot_by_fd(clientstate_t *states, int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (states[i].fd == fd) {
            return i;
        }
    }
    return -1;
}