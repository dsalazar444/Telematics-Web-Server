#ifndef MESSAGES_H
#define MESSAGES_H

#include "structs.h"

// Base de todos los mensajes
typedef struct {
    char msgId[37];      // UUID
    char type[32];
    long timestamp;
    char senderId[64];
} IMessage;

// LB → WS
typedef struct {
    IMessage    base;
    HttpRequest request;
    BackendNode targetNode;
    int         socketFd;
    long        enqueuedAt;
} ForwardRequestMessage;

// WS → Caché/Proxy
typedef struct {
    IMessage     base;
    HttpResponse response;
    HttpRequest  originalRequest;
    char         cacheKey[512];
    int          shouldCache;
    int          shouldReplicate;
    int          socketFd;
} BackendResponseMessage;

#endif