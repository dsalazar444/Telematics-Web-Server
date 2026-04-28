#ifndef HTTP_H
#define HTTP_H

#include "ISocket.h"  // para Socket*

// Nodo del cluster
typedef struct {
    char nodeId[64];
    char ip[64];
    int  port;
} BackendNode;

// Petición HTTP ya parseada
typedef struct {
    char method[16];
    char uri[2048];
    char version[16];
    char host[256];
} HttpRequest;

// Respuesta HTTP a construir
typedef struct {
    int  statusCode;
    char contentType[64];
    char *body;
    size_t bodyLen;
} HttpResponse;

// Cliente conectado (para uso interno del WS)
typedef struct {
    Socket*    socket;
    ClientInfo info;
} ConnectedClient;

#endif