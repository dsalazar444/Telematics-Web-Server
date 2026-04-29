#ifndef STRUCTS_H
#define STRUCTS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    CONNECT,
    TRACE
} HTTPMethod;

typedef struct {
    char key[256];
    char value[256];
} HTTPHeader;

typedef struct {
    HTTPHeader headers[100];
    size_t count;
} HTTPHeaders;

typedef struct {
    HTTPMethod method;
    char path[256];
    HTTPHeaders headers;
    unsigned char *body;
    char version[10];
} HTTPRequest;

typedef struct  {
    int  port;
    int  ttl;
    char** backends;
    uint16_t  backendCount;
} Config;

typedef struct {
    uint8_t ip[4];
    uint16_t port;
} IDBackendNode;

#endif // STRUCTS_H

