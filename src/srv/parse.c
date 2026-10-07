#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "common.h"
#include "parse.h"

int create_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0 || !headerOut) {
        return STATUS_ERROR;
    }

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        perror("[Server] calloc header");
        return STATUS_ERROR;
    }

    header->magic = HEADER_MAGIC;
    header->version = 0x1;
    header->count = 0;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;
    return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
	if (fd < 0 || !headerOut) {
        return STATUS_ERROR;
    }

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        perror("[Server] calloc header");
        return STATUS_ERROR;
    }

    if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("[Server] read header");
        free(header);
        return STATUS_ERROR;
    }

    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->filesize = ntohl(header->filesize);

    if (header->magic != HEADER_MAGIC) {
        printf("[Server] Improper header magic.\n");
        free(header);
        return STATUS_ERROR;
    }

    if (header->version != 1) {
        printf("[Server] Improper header version.\n");
        free(header);
        return STATUS_ERROR;
    }

    struct stat dbstat = {0};
    if (fstat(fd, &dbstat) == -1) {
        perror("[Server] fstat");
        free(header);
        return STATUS_ERROR;
    }

    if (header->filesize != dbstat.st_size) {
        printf("[Server] Corrupted database: file size mismatch.\n");
        free(header);
        return STATUS_ERROR;
    }

    *headerOut = header;
    return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
    if (fd < 0 || !dbhdr || !employeesOut) {
        return STATUS_ERROR;
    }

    int count = dbhdr->count;
    if (count == 0) {
        *employeesOut = NULL;
        return STATUS_SUCCESS;
    }

    struct employee_t *employees = calloc(count, sizeof(struct employee_t));
    if (employees == NULL) {
        perror("[Server] calloc employees");
        return STATUS_ERROR;
    }

    if (read(fd, employees, count * sizeof(struct employee_t)) != (ssize_t)(count * sizeof(struct employee_t))) {
        perror("[Server] read employees");
        free(employees);
        return STATUS_ERROR;
    }

    for (int i = 0; i < count; i++) {
        employees[i].hours = ntohl(employees[i].hours);
    }

    *employeesOut = employees;
    return STATUS_SUCCESS;
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring) {
    if (!dbhdr || !employees || !addstring) {
        return STATUS_ERROR;
    }

    char *name = strtok(addstring, ",");
    char *addr = strtok(NULL, ",");
    char *hours = strtok(NULL, ",");
    if (name == NULL || addr == NULL || hours == NULL) {
        printf("[Server] Malformed employee string. Expected format: name,address,hours\n");
        return STATUS_ERROR;
    }

    struct employee_t *new_employees = realloc(*employees, sizeof(struct employee_t) * (dbhdr->count + 1));
    if (new_employees == NULL) {
        perror("[Server] realloc");
        return STATUS_ERROR;
    }
    
    *employees = new_employees;
    int idx = dbhdr->count;

    memset(&new_employees[idx], 0, sizeof(struct employee_t));
    strncpy(new_employees[idx].name, name, sizeof(new_employees[idx].name) - 1);
    strncpy(new_employees[idx].address, addr, sizeof(new_employees[idx].address) - 1);
    new_employees[idx].hours = (unsigned int)atoi(hours);

    dbhdr->count++;
    dbhdr->filesize = sizeof(struct dbheader_t) + (dbhdr->count * sizeof(struct employee_t));

    return STATUS_SUCCESS;
}

int update_employee(struct dbheader_t *dbhdr, struct employee_t *employees, char *updatestring) {
    if (!dbhdr || !updatestring) {
        return STATUS_ERROR;
    }

    if (dbhdr->count == 0 || !employees) {
        printf("[Server] Database is empty.\n");
        return STATUS_ERROR;
    }

    char *name = strtok(updatestring, ",");
    char *addr = strtok(NULL, ",");
    char *hours = strtok(NULL, ",");

    if (name == NULL || addr == NULL || hours == NULL) {
        printf("[Server] Malformed update string. Expected: name,address,hours\n");
        return STATUS_ERROR;
    }

    for (int i = 0; i < dbhdr->count; i++) {
        if (strcmp(employees[i].name, name) == 0) {
            memset(employees[i].address, 0, sizeof(employees[i].address));
            strncpy(employees[i].address, addr, sizeof(employees[i].address) - 1);
            employees[i].hours = (unsigned int)atoi(hours);
            printf("[Server] Updated employee '%s'.\n", name);
            return STATUS_SUCCESS;
        }
    }

    printf("[Server] Employee '%s' not found for update.\n", name);
    return STATUS_ERROR;
}

int delete_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *target_name) {
    if (!dbhdr || !employees || !target_name) {
        return STATUS_ERROR;
    }
    
    if (dbhdr->count == 0 || !*employees) {
        printf("Database is empty.\n");
        return STATUS_ERROR;
    }

    int found_idx = -1;
    for (int i = 0; i < dbhdr->count; i++) {
        if (strcmp((*employees)[i].name, target_name) == 0) {
            found_idx = i;
            break;     
        }
    }

    if (found_idx == -1) {
        printf("[Server] Employee '%s' not found for deletion.\n", target_name);
        return STATUS_ERROR;
    }

    for (int i = found_idx; i < dbhdr->count - 1; i++) {
        (*employees)[i] = (*employees)[i + 1];
    }

    dbhdr->count--;

    if (dbhdr->count == 0) {
        free(*employees);
        *employees = NULL;
    } else {
        struct employee_t *temp = realloc(*employees, dbhdr->count * sizeof(struct employee_t));
        if (temp != NULL) {
            *employees = temp;
        }
    }

    return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees) {
	if (fd < 0 || !dbhdr) {
       return STATUS_ERROR;
    }

    int realcount = dbhdr->count;
    dbhdr->filesize = sizeof(struct dbheader_t) + realcount * sizeof(struct employee_t);

    struct dbheader_t proto_header;
    proto_header.magic = htonl(dbhdr->magic);
    proto_header.version = htons(dbhdr->version);
    proto_header.count = htons(dbhdr->count);
    proto_header.filesize = htonl(dbhdr->filesize);

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        perror("[Server] lseek");
        return STATUS_ERROR;
    }
    
    if (write(fd, &proto_header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("[Server] write header");
        return STATUS_ERROR;
    }

    for (int i = 0; i < realcount; i++) {
        struct employee_t emp_out = employees[i];
        emp_out.hours = htonl(emp_out.hours);
        if (write(fd, &emp_out, sizeof(struct employee_t)) != sizeof(struct employee_t)) {
            perror("[Server] write employee");
            return STATUS_ERROR;
        }
    }

    if (ftruncate(fd, dbhdr->filesize) == -1) {
        perror("[Server] ftruncate");
        return STATUS_ERROR;
    }
    
    return STATUS_SUCCESS;
}	




