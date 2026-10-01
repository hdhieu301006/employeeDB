#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -f <database file> [options]\n", argv[0]);
    printf("Options:\n");
    printf("\t -n                  Create new database file\n");
    printf("\t -f <file>           (Required) path to database file\n");
    printf("\t -a <name,addr,hrs>  Add employee\n");
    printf("\t -l                  List all employees\n");
    printf("\t -s <name>           Search employee by name\n");
    printf("\t -u <name,addr,hrs>  Update employee info\n");
    printf("\t -d <name>           Delete employee by name\n");
}

int main(int argc, char *argv[]) {
    char *filepath = NULL;
    char *addstring = NULL;
    char *search_name = NULL;
    char *updatestring = NULL;
    char *delete_name = NULL;
    bool newfile = false;
    bool list = false;
    int c;

    int dbfd = -1;
    struct dbheader_t *dbhdr = NULL;
    struct employee_t *employees = NULL;

    while ((c = getopt(argc, argv, "nf:a:ls:u:d:")) != -1) {
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
            case 'l':
                list = true;
                break;
            case 's':
                search_name = optarg;
                break;
            case 'u':
                updatestring = optarg;
                break;
            case 'd':
                delete_name = optarg;
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
            printf("Failed to read employees\n");
            close(dbfd);
            free(dbhdr);
            return STATUS_ERROR;
        }
    }

    if (addstring) {
        if (add_employee(dbhdr, &employees, addstring) == STATUS_ERROR) {
            printf("Failed to add employee\n");
            free(employees);
            free(dbhdr);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (list) {
        list_employees(dbhdr, employees);
    }

    if (search_name) {
        find_employee(dbhdr, employees, search_name);
    }

    if (updatestring) {
        if (update_employee(dbhdr, employees, updatestring) == STATUS_ERROR) {
            printf("Failed to update employee\n");
            free(employees);
            free(dbhdr);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (delete_name) {
        if (delete_employee(dbhdr, &employees, delete_name) == STATUS_ERROR) {
            printf("Failed to delete employee\n");
            free(employees);
            free(dbhdr);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (output_file(dbfd, dbhdr, employees) == STATUS_ERROR) {
        printf("Failed to write database\n");
        free(employees);
        free(dbhdr);
        close(dbfd);
        return STATUS_ERROR;
    }
    
    free(employees);
    free(dbhdr);
    close(dbfd);
    return STATUS_SUCCESS;
}
