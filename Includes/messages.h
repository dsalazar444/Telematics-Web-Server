#ifndef MESSAGES_H
#define MESSAGES_H

#include "http.h"
#include <stdbool.h>

// Mensaje de proxy: encapsula response, request, configuración de caché y replicación
typedef struct {
    HTTPResponse response;           // La respuesta HTTP a enviar
    const HTTPRequest *request;      // El request original del cliente
    char         cacheKey[33];       // Clave para el caché (MD5 hash)
    bool         shouldCache;        // Indica si debe cachearse la response
    bool         shouldReplicate;    // Indica si debe replicarse a otros servidores
} ProxyMessage;

#endif