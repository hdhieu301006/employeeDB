#ifndef PARSE_H
#define PARSE_H

#define HEADER_MAGIC 0x48494555

struct dbheader_t {
	uint32_t magic;
    uint16_t version;
	uint16_t count;
	uint32_t filesize;
};

struct employee_t {
	char name[256];
	char address[256];
	uint32_t hours;
};

int create_db_header(int fd, struct dbheader_t **headerOut);
int validate_db_header(int fd, struct dbheader_t **headerOut);
int read_employees(int fd, struct dbheader_t *, struct employee_t **employeesOut);
int output_file(int fd, struct dbheader_t *, struct employee_t *employees);
int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring);
void find_employee(struct dbheader_t *dbhdr, struct employee_t *employees, char *target_name);
int update_employee(struct dbheader_t *dbhdr, struct employee_t *employees, char *updatestring);
int delete_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *target_name);

#endif
