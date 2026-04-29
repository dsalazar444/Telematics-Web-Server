#ifndef MESSAGES_H
#define MESSAGES_H

#include "http.h"
#include "LoadBalancer/loadBalancer.h"

// WS → Caché/Proxy
typedef struct {
    HTTPResponse response;
    HTTPRequest  originalRequest;
    char         cacheKey[512];
    bool         shouldCache;
    bool         shouldReplicate;
} ProxyMessage;

#endif