#include "HttpParser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

static const char *FindHeader(const HTTPHeaders *headers, const char *key);

// Parsea un buffer de request HTTP y construye una estructura HTTPRequest
// Pide: buffer - datos recibidos; headerSize - tamaño de headers; contentLength
// Retorna: puntero a HTTPRequest si se parsea correctamente, NULL si hay error (y statusCode se establece con el código de error)
HTTPRequest *ParseHTTPRequest(const char *buffer, int headerSize, size_t contentLength, unsigned short *statusCode)
{
    HTTPRequest *request = malloc(sizeof(HTTPRequest));
    if (request == NULL) {
        *statusCode = 500;  // Internal Server Error
        return NULL;
    }
    memset(request, 0, sizeof(HTTPRequest));
    *statusCode = 0;  // Asumir válido por defecto

    const char *headersEnd = buffer + headerSize;

    const char *requestLineEnd = strstr(buffer, "\r\n");
    if (requestLineEnd == NULL || requestLineEnd > headersEnd)
    {
        *statusCode = 400;  // Bad Request porque no se encuentra \r\n
        free(request);
        return NULL;
    }

    char requestLine[1024];
    size_t requestLineLen = (size_t)(requestLineEnd - buffer);
    if (requestLineLen == 0 || requestLineLen >= sizeof(requestLine))
    {
        *statusCode = 400;  // Bad Request porque la línea de petición es demasiado larga o vacía
        free(request);
        return NULL;
    }
    memcpy(requestLine, buffer, requestLineLen);
    requestLine[requestLineLen] = '\0';

    // Obtiene metodo
    char method[16] = {0};
    if (sscanf(requestLine, "%15s %255s %9s", method, request->path, request->version) != 3)
    {
        *statusCode = 400;  // No puede obtener las partes de la línea de petición
        free(request);
        return NULL;
    }
    request->method = ParseMethod(method);

    if (request->method == -1)
    {
        *statusCode = 405;  // Method Not Allowed
        free(request);
        return NULL;
    }

    // Parsea la uri
    ParsedURI parsedURI = UriParse(request->path);
    if (!parsedURI._isValid)
    {
        *statusCode = parsedURI._statusCode != 0 ? parsedURI._statusCode : 400;  // Usar el código de estado específico si se estableció
        free(request);
        return NULL;
    }

    size_t pathLen = strlen(request->path);
    if (pathLen > 0 && request->path[pathLen - 1] == '/' && request->method != POST)
    {
        snprintf(request->path, sizeof(request->path), "%s/index.html", request->path);
    }

    // Parsea los headers y los agrupa
    char headersCopy[headerSize + 1];
    memcpy(headersCopy, buffer, headerSize); 
    headersCopy[headerSize] = '\0';

    const char *lineStart = strstr(headersCopy, "\r\n") + 2; 
    const char *headersEndCopy = headersCopy + headerSize;  

    while (lineStart < headersEndCopy)
    {
        const char *lineEnd = strstr(lineStart, "\r\n");
        if (lineEnd == NULL || lineEnd > headersEndCopy)
        {
            *statusCode = 400;  // Bad Request Header mal formulado
            free(request);
            return NULL;
        }

        if (lineEnd == lineStart)
        {
            break;
        }

        if (request->headers.count >= 100)
        {
            *statusCode = 431;  // Request Header Fields Too Large
            free(request);
            return NULL;
        }

        // Construye el header actual y lo agrega a la lista de headers
        HTTPHeader *header = &request->headers.headers[request->headers.count];

        const char *colon = memchr(lineStart, ':', (size_t)(lineEnd - lineStart));
        if (colon != NULL)
        {
            size_t keyLen = (size_t)(colon - lineStart);
            if (keyLen >= sizeof(header->key))
            {
                keyLen = sizeof(header->key) - 1;
            }
            memcpy(header->key, lineStart, keyLen);
            header->key[keyLen] = '\0';

            const char *valueStart = colon + 1;
            while (valueStart < lineEnd && (*valueStart == ' ' || *valueStart == '\t'))
            {
                valueStart++;
            }

            size_t valueLen = (size_t)(lineEnd - valueStart);
            if (valueLen >= sizeof(header->value))
            {
                valueLen = sizeof(header->value) - 1;
            }
            memcpy(header->value, valueStart, valueLen);
            header->value[valueLen] = '\0';

            request->headers.count++;
        }

        lineStart = lineEnd + 2;
    }

    // Verifica la version del protocolo
    if (strcmp(request->version, "HTTP/1.1") == 0 && FindHeader(&request->headers, "Host") == NULL)
    {
        *statusCode = 400;  // HTTP/1.1 requiere Host
        free(request);
        return NULL;
    }

    request->body = NULL;
    request->bodyLength = 0; 

    // Lee el body y lo asigna al buffer y lo agrega a la estructura HTTPRequest
    if (contentLength > 0 && contentLength < 1024 * 1024) 
    {
        const unsigned char *bodyStart = (const unsigned char *)buffer + headerSize;
        request->body = malloc(contentLength); 
        if (request->body == NULL)
        {
            *statusCode = 500;  // Internal Server Error
            free(request);
            return NULL;
        }

        memcpy(request->body, bodyStart, contentLength); 
        request->bodyLength = contentLength; 
    }

    return request;
}

// Obtiene un header específico por su nombre (case-insensitive) de una lista de headers
static const char *FindHeader(const HTTPHeaders *headers, const char *key)
{
    for (size_t i = 0; i < headers->count; i++)
    {
        if (strcasecmp(headers->headers[i].key, key) == 0)
        {
            return headers->headers[i].value;
        }
    }

    return NULL;
}

// Trasforma un string de método HTTP en su valor enum correspondiente
HTTPMethod ParseMethod(const char *method)
{
    if (strcasecmp(method, "GET") == 0)
        return GET;
    if (strcasecmp(method, "POST") == 0)
        return POST;
    if (strcasecmp(method, "PUT") == 0)
        return PUT;
    if (strcasecmp(method, "DELETE") == 0)
        return DELETE;
    if (strcasecmp(method, "HEAD") == 0)
        return HEAD;
    if (strcasecmp(method, "OPTIONS") == 0)
        return OPTIONS;
    if (strcasecmp(method, "CONNECT") == 0)
        return CONNECT;
    if (strcasecmp(method, "TRACE") == 0)
        return TRACE;
    return -1;
}