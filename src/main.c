#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -n -f <database file>\n", argv[0]);
    printf("\t -n - create new database file\n");
    printf("\t -f - (required) path to database file\n");
    return;
}

int main(int argc, char *argv[]) {
    char *filepath = NULL; 
    bool newfile = false;
    int c;

    int dbfd = -1;
    struct dbheader_t *dbhdr = NULL;


    while ((c = getopt(argc, argv, "nf:")) != -1) {
        switch (c) {
            case 'n':
                newfile = true;
                break;
            case 'f':
                filepath = optarg;
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
            return -1;
        }
    } else {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            return STATUS_ERROR;
        }

        if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            printf("Failed to validate database header\n");
            close(dbfd);
            return -1;
        }
    }

    if (output_file(dbfd, dbhdr) == STATUS_ERROR) {
        printf("Failed to write database\n");
    }
    
    free(dbhdr);
    close(dbfd);
    return STATUS_SUCCESS;
}
