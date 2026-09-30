#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -f <database file> [-n] [-a <employee_data>]\n", argv[0]);
    printf("\t -n - create new database file\n");
    printf("\t -f - (required) path to database file\n");
    printf("\t -a - add employee: \"name, address,hours\"\n");
}

int main(int argc, char *argv[]) {
    char *filepath = NULL;
    char *addstring = NULL;
    bool newfile = false;
    int c;

    int dbfd = -1;
    struct dbheader_t *dbhdr = NULL;
    struct employee_t *employees = NULL;

    while ((c = getopt(argc, argv, "nf:a:")) != -1) {
        switch (c) {
            case 'n':
                newfile = true;
                break;
            case 'f':
                filepath = optarg;
                break;
            case 'a':
                addstring = optarg;
                break;
            case '?':
                print_usage(argv);
                return STATUS_ERROR;
            default:
                return STATUS_ERROR;      
        }
    }

    if (filepath == NULL) {
        printf("Filepath is a required argument\n");
        print_usage(argv);

        return STATUS_ERROR;
    }

    if (newfile) {
        dbfd = create_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            return STATUS_ERROR;
        }

        if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            printf("Failed to create database header\n");
            close(dbfd);
            return STATUS_ERROR;
        }
    } else {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            return STATUS_ERROR;
        }

        if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            printf("Failed to validate database header\n");
            close(dbfd);
            return STATUS_ERROR;
        }

        if (read_employees(dbfd, dbhdr, &employees) == STATUS_ERROR) {
            printf("Failed to read employees");
            close(dbfd);
            free(dbhdr);
            return STATUS_ERROR;
        }
    }

    if (addstring) {
        dbhdr->count++;
        struct employee_t *temp = realloc(employees, dbhdr->count*(sizeof(struct employee_t)));
        if (temp == NULL) {
            perror("realloc");
            free(employees);
            free(dbhdr);
            close(dbfd);
            return STATUS_ERROR;
        }
        employees = temp;
        if (add_employee(dbhdr, employees, addstring)) {
            free(employees);
            free(dbhdr);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (output_file(dbfd, dbhdr, employees) == STATUS_ERROR) {
        printf("Failed to write database\n");
    }
    
    free(employees);
    free(dbhdr);
    close(dbfd);
    return STATUS_SUCCESS;
}
