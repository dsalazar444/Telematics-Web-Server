#ifndef HTTP_H
#define HTTP_H

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

// Colección de headers HTTP
typedef struct {
    HTTPHeader headers[100];
    size_t count;
} HTTPHeaders;

// Estructura de un request HTTP
typedef struct {
    HTTPMethod method;
    char path[256];
    HTTPHeaders headers;
    unsigned char *body;
    size_t bodyLength; 
    char version[10];
} HTTPRequest;

// Estructura de una response HTTP
typedef struct {
    int statusCode;
    char statusMessage[64];
    HTTPHeaders headers;
    unsigned char *body;
    size_t bodyLength; 
} HTTPResponse;

#endif


