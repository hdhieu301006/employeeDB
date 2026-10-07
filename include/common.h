#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define STATUS_ERROR   -1
#define STATUS_SUCCESS 0

#define PROTO_VER 100

typedef enum {
	MSG_HELLO_REQ,
	MSG_HELLO_RESP,
	MSG_EMPLOYEE_LIST_REQ,
	MSG_EMPLOYEE_LIST_RESP,
	MSG_EMPLOYEE_ADD_REQ,
	MSG_EMPLOYEE_ADD_RESP,
	MSG_EMPLOYEE_UPDATE_REQ,
	MSG_EMPLOYEE_UPDATE_RESP,
	MSG_EMPLOYEE_DEL_REQ,
	MSG_EMPLOYEE_DEL_RESP,
	MSG_ERROR,
} dbproto_type_e;

typedef struct {
	uint16_t type;
	uint16_t len;
} __attribute__((packed)) dbproto_hdr_t;

typedef struct {
	uint16_t proto;
} __attribute__((packed)) dbproto_hello_req;

typedef struct {
	uint16_t proto;
} __attribute__((packed)) dbproto_hello_resp;

typedef struct {
	char data[1024];
} __attribute__((packed)) dbproto_employee_add_req;

typedef struct {
	char name[256];
	char address[256];
	uint32_t hours;
} __attribute__((packed)) dbproto_employee_list_resp;

typedef struct {
	char data[1024];
} __attribute__((packed)) dbproto_employee_update_req;

typedef struct {
	char name[256];
} __attribute__((packed)) dbproto_employee_del_req;

typedef struct {
	int16_t error_code;
	char message[256];
} __attribute__((packed)) dbproto_error_resp;

#endif
