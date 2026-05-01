#ifndef MESSAGES_H
#define MESSAGES_H

#include "http.h"
#include <stdbool.h>

typedef struct {
    HTTPResponse response;
    const HTTPRequest *request;
    char         cacheKey[512];
    bool         shouldCache;
    bool         shouldReplicate;
} ProxyMessage;

#endif