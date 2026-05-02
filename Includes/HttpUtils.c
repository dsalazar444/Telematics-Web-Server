#include "HttpUtils.h"
#include <strings.h>
#include <stdlib.h>
#include <string.h>

const char* GetHeaderValue(const HTTPHeaders* headers, const char* key) {
    if (headers == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < headers->count; i++) {
        if (strcasecmp(headers->headers[i].key, key) == 0) {
            return headers->headers[i].value;
        }
    }
    return NULL;
}

void ResponseFree(HTTPResponse* response) {
    if (response) {
        if (response->body) {
            free(response->body);
            response->body = NULL;
        }
        free(response);
    }
}

void RequestFree(HTTPRequest* request) {
    if (request) {
        if (request->body) {
            free(request->body);
            request->body = NULL;
        }
        free(request);
    }
}


// analiza buffer de la petición, y mira donde donde terminan los headers, y el tamaño del body, si lo incluye
// retorna: 0 -> no hay final de headers -> no hay secuencia \r\n\r\
// -1 -> si hay errores de formato (headers mal formateados, etc.)
// 1 -> ok
int GetRequestSizes(const char *requestBuffer, int *headerSize, int *contentLength)
{
    const char *headersEnd = strstr(requestBuffer, "\r\n\r\n");
    if (headersEnd == NULL)
    {
        return 0;
    }

    *headerSize = (int)(headersEnd - requestBuffer) + 4;
    *contentLength = 0;
    const char *lineStart = strstr(requestBuffer, "\r\n");
    if (lineStart == NULL || lineStart >= headersEnd)
    {
        return -1;
    }
    lineStart += 2;

    while (lineStart < headersEnd)
    {
        const char *lineEnd = strstr(lineStart, "\r\n");
        if (lineEnd == NULL || lineEnd > headersEnd)
        {
            return -1;
        }
        if (lineEnd == lineStart)
        {
            break;
        }

        const char *colon = memchr(lineStart, ':', (size_t)(lineEnd - lineStart));
        if (colon != NULL)
        {
            size_t keyLen = (size_t)(colon - lineStart);
            if (keyLen == strlen("Content-Length") && strncasecmp(lineStart, "Content-Length", keyLen) == 0)
            {
                const char *valueStart = colon + 1;
                while (valueStart < lineEnd && (*valueStart == ' ' || *valueStart == '\t'))
                {
                    valueStart++;
                }

                char valueBuffer[32];
                size_t valueLen = (size_t)(lineEnd - valueStart);
                if (valueLen == 0 || valueLen >= sizeof(valueBuffer))
                {
                    return -1;
                }

                memcpy(valueBuffer, valueStart, valueLen);
                valueBuffer[valueLen] = '\0';

                char *endPtr = NULL;
                long parsed = strtol(valueBuffer, &endPtr, 10);
                if (*endPtr != '\0' || parsed < 0 || parsed > 1024 * 1024)
                {
                    return -1;
                }

                *contentLength = (int)parsed;
                return 1;
            }
        }

        lineStart = lineEnd + 2;
    }

    return 1;
}