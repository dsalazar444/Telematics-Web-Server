#include "HttpUtils.h"
#include <strings.h>
#include <stdlib.h>
#include <string.h>

// Busca el valor de un header en la lista por nombre (case-insensitive)
// Pide: headers - lista de headers; key - nombre del header a buscar
// Retorna: valor del header si existe, NULL si no
const char* GetHeaderValue(const HTTPHeaders* headers, const char* key) {
    if (headers == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < headers->count; i++) {
        if (strcasecmp(headers->headers[i].key, key) == 0) {
            return headers->headers[i].value;
        }
    }
    return NULL;
}

// Obtiene la frase de razón HTTP para un código de estado
// Pide: statusCode - código HTTP (200, 404, 500, etc.)
// Retorna: string con la frase descriptiva (ej: "OK", "Not Found")
const char* GetReasonPhrase(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 411: return "Length Required";
        case 414: return "Request-URI Too Long";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}

// Libera la memoria de una respuesta HTTP
// Pide: response - puntero a HTTPResponse a liberar
// Retorna: nada
void ResponseFree(HTTPResponse* response) {
    if (response) {
        if (response->body) {
            free(response->body);
            response->body = NULL;
        }
        free(response);
    }
}

// Libera la memoria de un request HTTP
// Pide: request - puntero a HTTPRequest a liberar
// Retorna: nada
void RequestFree(HTTPRequest* request) {
    if (request) {
        if (request->body) {
            free(request->body);
            request->body = NULL;
        }
        free(request);
    }
}


// Analiza un buffer de request y extrae tamaño de headers y body
// Pide: requestBuffer - buffer con datos recibidos; headerSize - para retornar tamaño headers; contentLength - para retornar tamaño body
// Retorna: 1 si ok, 0 si headers incompletos, -1 si formato inválido
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