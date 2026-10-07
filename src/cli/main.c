#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -h <host> -p <port> [options]\n", argv[0]);
    printf("Options:\n");
    printf("\t -h <host>                (Required) server IP address\n");
    printf("\t -p <port>                (Required) server port\n");
    printf("\t -a <\"name,addr,hrs\">   Add new employee\n");
    printf("\t -l                       List all employees\n");
    printf("\t -u <\"name,addr,hrs\">   Update an existing employee\n");
    printf("\t -d <name>                Delete an employee\n");
}

int send_hello(int fd) {
    char buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_req)] = {0};

    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;
    hdr->type = htons(MSG_HELLO_REQ);
    hdr->len = htons(sizeof(dbproto_hello_req));

    dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
    hello->proto = htons(PROTO_VER);

    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("[Client] write hello");
        return STATUS_ERROR;
    }

    dbproto_hdr_t resp_hdr;
    if (read(fd, &resp_hdr, sizeof(resp_hdr)) != sizeof(resp_hdr)) {
        perror("[Client] read hello header");
        return STATUS_ERROR;
    }

    resp_hdr.type = ntohs(resp_hdr.type);
    resp_hdr.len = ntohs(resp_hdr.len);

    if (resp_hdr.type == MSG_ERROR) {
        printf("[Client] Server returned protocol error.\n");
        return STATUS_ERROR;
    }

    if (resp_hdr.type != MSG_HELLO_RESP) {
        printf("[Client] Unexpected hello response type: %d\n", resp_hdr.type);
        return STATUS_ERROR;
    }

    dbproto_hello_resp resp_body;
    if (read(fd, &resp_body, sizeof(resp_body)) != sizeof(resp_body)) {
        perror("[Client] read hello body");
        return STATUS_ERROR;
    }

    if (ntohs(resp_body.proto) != PROTO_VER) {
        printf("[Client] Protocol version mismatch: %d\n", ntohs(resp_body.proto));
        return STATUS_ERROR;
    }

    printf("[Client] Handshake successful (Protocol v%d).\n", PROTO_VER);
    return STATUS_SUCCESS;
}

int send_employee_add(int fd, char *addstr) {
    char buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_req)] = {0};

    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;
    hdr->type = htons(MSG_EMPLOYEE_ADD_REQ);
    hdr->len = htons(sizeof(dbproto_employee_add_req));

    dbproto_employee_add_req *employee = (dbproto_employee_add_req *)&hdr[1];
    strncpy(employee->data, addstr, sizeof(employee->data) - 1);

    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("[Client] write employee add");
        return STATUS_ERROR;
    }

    dbproto_hdr_t resp_hdr;
    if (read(fd, &resp_hdr, sizeof(resp_hdr)) != sizeof(resp_hdr)) {
        perror("[Client] read add header");
        return STATUS_ERROR;
    }

    resp_hdr.type = ntohs(resp_hdr.type);

    if (resp_hdr.type == MSG_ERROR) {
        printf("[Client] Failed to add employee.\n");
        return STATUS_ERROR;
    }

    if (resp_hdr.type == MSG_EMPLOYEE_ADD_RESP) {
        printf("[Client] Employee added successfully.\n");
        return STATUS_SUCCESS;
    }

    printf("[Client] Unexpected response type: %d\n", resp_hdr.type);
    return STATUS_ERROR;
}

int list_employees(int fd) {
    dbproto_hdr_t req_hdr;
    req_hdr.type = htons(MSG_EMPLOYEE_LIST_REQ);
    req_hdr.len = htons(0);

    if (write(fd, &req_hdr, sizeof(req_hdr)) != sizeof(req_hdr)) {
        perror("[Client] write list request");
        return STATUS_ERROR;
    }
    
    dbproto_hdr_t resp_hdr;
    if (read(fd, &resp_hdr, sizeof(resp_hdr)) != sizeof(resp_hdr)) {
        perror("[Client] read list header");
        return STATUS_ERROR;
    }
    
    resp_hdr.type = ntohs(resp_hdr.type);
    resp_hdr.len = ntohs(resp_hdr.len);

    if (resp_hdr.type == MSG_ERROR) {
        printf("[Client] Unable to list employees.\n");
        return STATUS_ERROR;
    }

    if (resp_hdr.type != MSG_EMPLOYEE_LIST_RESP) {
        printf("[Client] Unexpected response type: %d\n", resp_hdr.type);
        return STATUS_ERROR;
    }

    if (resp_hdr.len == 0) {
        printf("[Client] Database is currently empty.\n");
        return STATUS_SUCCESS;
    }

    uint16_t total_bytes = resp_hdr.len;
    dbproto_employee_list_resp *employees = malloc(total_bytes);
    if (!employees) {
        perror("[Client] malloc");
        return STATUS_ERROR;
    }

    uint16_t bytes_read = 0;
    while (bytes_read < total_bytes) {
        ssize_t res = read(fd, (char *)employees + bytes_read, total_bytes - bytes_read);
        if (res <= 0) {
            perror("[Client] read list body failed");
            free(employees);
            return STATUS_ERROR;
        }
        bytes_read += (uint16_t)res;
    }

    int count = total_bytes / sizeof(dbproto_employee_list_resp);
    printf("[Client] --- Employee List (%d records) ---\n", count);
    for (int i = 0; i < count; i++) {
        uint32_t hours = ntohl(employees[i].hours);
        printf("Employee %d:\n", i);
        printf("\tName:     %s\n", employees[i].name);
        printf("\tAddress:  %s\n", employees[i].address);
        printf("\tHours:    %u\n", employees[i].hours);
    }

    free(employees);
    return STATUS_SUCCESS;
}

int send_employee_update(int fd, char *updatestr) {
    char buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_update_req)] = {0};
    
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;
    hdr->type = htons(MSG_EMPLOYEE_UPDATE_REQ);
    hdr->len = htons(sizeof(dbproto_employee_update_req));
    
    dbproto_employee_update_req *employee = (dbproto_employee_update_req *)&hdr[1];
    strncpy(employee->data, updatestr, sizeof(employee->data) - 1);
    
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("[Client] write employee update");
        return STATUS_ERROR;
    }
    
    dbproto_hdr_t resp_hdr;
    if (read(fd, &resp_hdr, sizeof(resp_hdr)) != sizeof(resp_hdr)) {
        perror("[Client] read update header");
        return STATUS_ERROR;
    }

     resp_hdr.type = ntohs(resp_hdr.type);

    if (resp_hdr.type == MSG_ERROR) {
        printf("[Client] Failed to update employee.\n");
        return STATUS_ERROR;
    }

    if (resp_hdr.type == MSG_EMPLOYEE_UPDATE_RESP) {
        printf("[Client] Employee updated successfully.\n");
        return STATUS_SUCCESS;
    }

    printf("[Client] Unexpected response type: %d\n", resp_hdr.type);
    return STATUS_ERROR;
}

int send_employee_delete(int fd, char *target_name) {
    char buf[sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_del_req)] = {0};
    
    dbproto_hdr_t *hdr = (dbproto_hdr_t *)buf;
    hdr->type = htons(MSG_EMPLOYEE_DEL_REQ);
    hdr->len = htons(sizeof(dbproto_employee_del_req));

    dbproto_employee_del_req *del = (dbproto_employee_del_req *)&hdr[1];
    strncpy(del->name, target_name, sizeof(del->name) - 1);

    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("[Client] write employee delete");
        return STATUS_ERROR;
    }

    dbproto_hdr_t resp_hdr;
    if (read(fd, &resp_hdr, sizeof(resp_hdr)) != sizeof(resp_hdr)) {
        perror("[Client] read delete header");
        return STATUS_ERROR;
    }

    resp_hdr.type = ntohs(resp_hdr.type);

    if (resp_hdr.type == MSG_ERROR) {
        printf("[Client] Failed to delete employee (Employee not found).\n");
        return STATUS_ERROR;
    }

    if (resp_hdr.type == MSG_EMPLOYEE_DEL_RESP) {
        printf("[Client] Employee deleted successfully.\n");
        return STATUS_SUCCESS;
    }

    printf("[Client] Unexpected response type: %d\n", resp_hdr.type);
    return STATUS_ERROR;
}

int main(int argc, char *argv[]) {
    char *addarg = NULL;
    char *updatearg = NULL;
    char *delarg = NULL;
    char *portarg = NULL;
    char *hostarg = NULL;
    unsigned short port = 0;
    bool list = false;
    int c;

    while ((c = getopt(argc, argv, "h:p:a:lu:d:")) != -1) {
        switch (c) {
            case 'h':
                hostarg = optarg;
                break;
            case 'p':
                portarg = optarg;
                port = (unsigned short)atoi(portarg);
                break;
            case 'a':
                addarg = optarg;
                break;
            case 'l':
                list = true;
                break;
            case 'u':
                updatearg = optarg;
                break;
            case 'd':
                delarg = optarg;
                break;
            case '?':
                print_usage(argv);
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }

    if (port == 0) {
        printf("[Client] Bad port: %s\n", portarg ? portarg : "NULL");
        print_usage(argv);
        return EXIT_FAILURE;
    }

    if (hostarg == NULL) {
        printf("[Client] Must specify host with -h\n");
        return EXIT_FAILURE;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("[Client] socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, hostarg, &server_addr.sin_addr) <= 0) {
        perror("[Client] inet_pton");
        close(fd);
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("[Client] connect");
        close(fd);
        return EXIT_FAILURE;
    }

    if (send_hello(fd) != STATUS_SUCCESS) {
        close(fd);
        return EXIT_FAILURE;
    }

    if (addarg) {
        send_employee_add(fd, addarg);
    }

    if (updatearg) {
        send_employee_update(fd, updatearg);
    }

    if (delarg) {
        send_employee_delete(fd, delarg);
    }

    if (list) {
        list_employees(fd);
    }

    close(fd);
    return EXIT_SUCCESS;
}