#include "HttpParser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

static const char *FindHeader(const HTTPHeaders *headers, const char *key);

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

    // Agregar uriParser
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

    if (strcmp(request->version, "HTTP/1.1") == 0 && FindHeader(&request->headers, "Host") == NULL)
    {
        *statusCode = 400;  // HTTP/1.1 requiere Host
        free(request);
        return NULL;
    }

    request->body = NULL;
    request->bodyLength = 0; 

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

// Helper: convertir HTTPMethod enum a string
// Pide: method - valor del enum HTTPMethod
// Retorna: cadena de texto correspondiente al método HTTP, o NULL si el método es desconocido
static const char *MethodToString(HTTPMethod method)
{
    switch (method)
    {
    case GET:
        return "GET";
    case POST:
        return "POST";
    case PUT:
        return "PUT";
    case DELETE:
        return "DELETE";
    case HEAD:
        return "HEAD";
    case OPTIONS:
        return "OPTIONS";
    case CONNECT:
        return "CONNECT";
    case TRACE:
        return "TRACE";
    default:
        return NULL;
    }
}

// Serializa un HTTPRequest a un buffer de texto para enviar por socket
// Pide: request - puntero al HTTPRequest a serializar; buffer - buffer donde se va a escribir la representación textual; bufferSize - tamaño del buffer
// Retorna: número de bytes escritos en el buffer, o -1 si hay error (parámetros inválidos o buffer insuficiente)
int SerializeHTTPRequest(const HTTPRequest *request, char *buffer, int bufferSize)
{
    if (request == NULL || buffer == NULL || bufferSize <= 0)
    {
        fprintf(stderr, "SerializeHTTPRequest: parámetros inválidos\n");
        return -1;
    }

    const char *methodStr = MethodToString(request->method);
    if (methodStr == NULL)
    {
        fprintf(stderr, "SerializeHTTPRequest: HTTPMethod desconocido (%d)\n", request->method);
        return -1;
    }

    size_t offset = 0;

    // PASO 1: Construir línea de petición: "METHOD /path HTTP/version\r\n"
    offset += snprintf(buffer + offset, bufferSize - offset,
                       "%s %s %s\r\n",
                       methodStr, request->path, request->version);

    if (offset >= (size_t)bufferSize)
    {
        fprintf(stderr, "SerializeHTTPRequest: buffer insuficiente para línea inicial\n");
        return -1;
    }

    // PASO 2: Añadir headers
    for (size_t i = 0; i < request->headers.count; i++)
    {
        // Verificar que no nos salimos del buffer
        if (offset + 512 >= (size_t)bufferSize)
        {
            fprintf(stderr, "SerializeHTTPRequest: buffer insuficiente para headers\n");
            return -1;
        }

        // Añadir: "Key: Value\r\n"
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "%s: %s\r\n",
                           request->headers.headers[i].key,
                           request->headers.headers[i].value);
    }

    // PASO 3: Añadir línea separadora vacía: "\r\n"
    offset += snprintf(buffer + offset, bufferSize - offset, "\r\n");

    return (int)offset;
}