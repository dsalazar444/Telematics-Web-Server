// Worker.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "Worker.h"
#include "../Socket/Socket.h"
#include "../HttpParser/HttpParser.h"



void *worker(void *arg)
{
    WorkerArgs *workerArgs = (WorkerArgs*)arg;

    IClientSocket *client = workerArgs->client;
    LoadBalancer *lb = workerArgs->lb;

    // Recibir la petición
    char buffer[4096];

    char requestBuffer[4096];
    int requestLen = 0;
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));
        if (bytes <= 0) break;

        // Acumular en request_buffer
        if (requestLen + bytes >= sizeof(requestBuffer)) {
            printf("Error: petición demasiado grande\n");
            break;
        }
        memcpy(requestBuffer + requestLen, buffer, bytes);
        requestLen += bytes;
        requestBuffer[requestLen] = '\0';

        int headerSize = 0;
        int contentLength = 0;
        int requestInfoStatus = GetRequestSizes(requestBuffer, &headerSize, &contentLength);
        if (requestInfoStatus == 0)
        {
            continue;
        }

        if (requestInfoStatus < 0)
        {
            char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            SendToClient(client, bad_request, strlen(bad_request));
            break;
        }
        int expectedSize = headerSize + contentLength;

        if (requestLen < expectedSize)
        {
            continue;
        }

        // Ya tenemos la petición completa (headers)
        HTTPRequest *request = ParseHTTPRequest(requestBuffer, headerSize, contentLength);
        if (request == NULL)
        {
            printf("Error: No se pudo parsear la petición HTTP\n");
            requestLen = 0;
            memset(requestBuffer, 0, sizeof(requestBuffer));
            continue;
        }
        PrintHttpRequest(request);

        ConnectToBackendAndForward(workerArgs, request);

        char *response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHola cliente\r\n";
        SendToClient(client, response, strlen(response));

        // Limpiar buffer para la siguiente petición
        requestLen = 0;
        memset(requestBuffer, 0, sizeof(requestBuffer));
    }

    // 4. Cerrar y liberar
    CloseClientSocket(client);
    return NULL;
}

void ConnectToBackendAndForward(WorkerArgs *workerArgs, const HTTPRequest *request){
    // Aquí se implementaría la lógica para conectar al backend seleccionado por el LoadBalancer
    // y reenviar la petición. Esto incluiría:
    // 1. Seleccionar un backend usando LoadBalancerSelectBackend(workerArgs->lb)
    // 2. Crear un socket para conectarse al backend
    // 3. Conectar al backend usando la IP y puerto del BackendNode seleccionado
    // 4. Enviar la petición al backend
    // 5. Recibir la respuesta del backend
    // 6. Reenviar la respuesta al cliente original usando SendToClient(workerArgs->client, ...)
    LoadBalancer *lb = workerArgs->lb;
    BackendNode backend = LoadBalancerSelectBackend(lb);
    // yo  genero el proxymessage
    IncrementActiveConnections(lb, &backend);
}

void PrintHttpRequest(const HTTPRequest *request) {
    printf("Method: %d\n", request->method);
    printf("Path: %s\n", request->path);
    printf("Version: %s\n", request->version);
    printf("Headers:\n");
    for (size_t i = 0; i < request->headers.count; i++) {
        printf("  %s: %s\n", request->headers.headers[i].key, request->headers.headers[i].value);
    }
    if (request->body) {
        printf("Body length: %zu\n", request->bodyLength); 
    } else {
        printf("No Body\n");
    }
}

static int GetRequestSizes(const char *requestBuffer, int *headerSize, int *contentLength)
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

void PrintHttpRequest(const HTTPRequest *request) {
    printf("Method: %d\n", request->method);
    printf("Path: %s\n", request->path);
    printf("Version: %s\n", request->version);
    printf("Headers:\n");
    for (size_t i = 0; i < request->headers.count; i++) {
        printf("  %s: %s\n", request->headers.headers[i].key, request->headers.headers[i].value);
    }
    if (request->body) {
        printf("Body length: %zu\n", request->bodyLength); 
    } else {
        printf("No Body\n");
    }
}

static int GetRequestSizes(const char *requestBuffer, int *headerSize, int *contentLength)
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