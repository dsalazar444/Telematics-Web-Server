#include "ProxyWorker.h"
#include "../HttpParser/HttpParser.h"
#include "../HttpServer/src/Response.h"
#include "../HttpServer/src/ResponseSender.h"
#include "../Replicator/Replicator.h"
#include "../../modules/Logs/Log.h"
#include "../../Includes/HttpUtils.h"

#define LEVEL "Cache"

void *worker(void *arg)
{
    WorkerArgs *workerArgs = (WorkerArgs *)arg;
    IClientSocket *client = workerArgs->client;
    LoadBalancer *lb = workerArgs->lb;
    CacheManager *cacheManager = workerArgs->cacheManager;
    int logFile = workerArgs->logFile;

    char buffer[4096];
    char requestBuffer[4096];
    int requestLen = 0;

    while (1)
    {
        // 1. Recibir datos del cliente
        memset(buffer, 0, sizeof(buffer));
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));
        if (bytes <= 0)
            break;

        if (requestLen + bytes >= sizeof(requestBuffer))
        {
            printf("Error: petición demasiado grande\n");
            break;
        }
        memcpy(requestBuffer + requestLen, buffer, bytes);
        requestLen += bytes;
        requestBuffer[requestLen] = '\0';

        // 2. Verificar tamaños
        int headerSize = 0;
        int contentLength = 0;
        int requestInfoStatus = GetRequestSizes(requestBuffer, &headerSize, &contentLength);
        if (requestInfoStatus == 0)
            continue;

        if (requestInfoStatus < 0)
        {
            char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            SendToClient(client, bad_request, strlen(bad_request));
            break;
        }

        int expectedSize = headerSize + contentLength;
        if (requestLen < expectedSize)
            continue;

        // 3. Parsear el request
        unsigned short statusCode = 0;
        HTTPRequest *request = ParseHTTPRequest(requestBuffer, headerSize, contentLength, &statusCode);
        if (request == NULL || statusCode != 0)
        {
            HTTPResponse *response = ResponseError((int)statusCode);
            if (response != NULL)
            {
                SendHTTPResponse(client, response);
                ResponseFree(response);
            }
            else
            {
                const char *fallback = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\nConnection: close\r\n\r\nInternal Server Error";
                SendToClient(client, fallback, strlen(fallback));
            }
            requestLen = 0;
            memset(requestBuffer, 0, sizeof(requestBuffer));
            continue;
        }

        PrintHttpRequest(request);
        const char *requestString = HttpRequestToString(request);
        LogWrite(logFile, LEVEL, requestString);

        // 4. Allocar ProxyMessage
        ProxyMessage *proxyMessage = malloc(sizeof(*proxyMessage));
        if (proxyMessage == NULL)
        {
            fprintf(stderr, "Error al allocar ProxyMessage\n");
            const char *fallback = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\nConnection: close\r\n\r\nInternal Server Error";
            SendToClient(client, fallback, strlen(fallback));
            free(request->body);
            free(request);
            requestLen = 0;
            memset(requestBuffer, 0, sizeof(requestBuffer));
            continue;
        }

        memset(proxyMessage, 0, sizeof(*proxyMessage));
        proxyMessage->request = request;
        proxyMessage->shouldCache = false;
        proxyMessage->shouldReplicate = (request->method == POST);

        // 5. Verificar si es cacheable y buscar en caché
        if (cacheManager != NULL &&
            cacheKeyFromRequest(request, proxyMessage->cacheKey, sizeof(proxyMessage->cacheKey)))
        {
            proxyMessage->shouldCache = true;

            HTTPResponse cachedResponse = {0};
            if (CacheLookUp(cacheManager, proxyMessage->cacheKey, &cachedResponse))
            {
                // HIT — enviar al cliente directo
                SendHTTPResponse(client, &cachedResponse);
                free(cachedResponse.body);
                free(proxyMessage);
                free(request->body);
                free(request);
                requestLen = 0;
                memset(requestBuffer, 0, sizeof(requestBuffer));
                continue;
            }
        }

        // 6. MISS — ir al backend
        ConnectToBackendAndForward(workerArgs, proxyMessage);

        // 7. Enviar respuesta al cliente
        SendHTTPResponse(client, &proxyMessage->response);

        // 8. Invalidar caché si es POST exitoso
        if (request->method == POST && proxyMessage->response.statusCode == 200)
        {
            if (cacheManager != NULL)
            {
                CacheInvalidateByRequest(cacheManager, request);
            }
        }

        // 9. Cachear en background si aplica
        bool bodyOwnedByCacheThread = false;
        if (proxyMessage->shouldCache && proxyMessage->response.statusCode == 200)
        {
            CacheStoreAsync(cacheManager, proxyMessage);
            bodyOwnedByCacheThread = true;
        }

        // 10. Liberar
        if (!bodyOwnedByCacheThread)
        {
            free(proxyMessage->response.body);
        }
        free(proxyMessage);
        free(request->body);
        free(request);
        requestLen = 0;
        memset(requestBuffer, 0, sizeof(requestBuffer));
    }

    // 11. Cerrar conexión
    CloseClientSocket(client);
    free(workerArgs);
    return NULL;
}

void ConnectToBackendAndForward(WorkerArgs *workerArgs, ProxyMessage *message)
{
    if (workerArgs == NULL || message == NULL || message->request == NULL)
    {
        return;
    }

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
    IClientSocket *backendSocket = CreateClientSocket(backend.id.ip, backend.id.port, 5000);
    if (backendSocket == NULL)
    {
        message->response.statusCode = 502;
        snprintf(message->response.statusMessage, sizeof(message->response.statusMessage), "%s", "Bad Gateway");
        message->response.headers.count = 0;
        message->response.body = NULL;
        message->response.bodyLength = 0;
        message->shouldCache = false;
        message->shouldReplicate = false;
        return;
    }

    IncrementActiveConnections(lb, &backend);
    if (SendHTTPRequest(backendSocket, message->request) < 0)
    {
        fprintf(stderr, "Error al enviar la petición al backend\n");
        DecrementActiveConnections(lb, &backend);
        CloseClientSocket(backendSocket);
        message->response.statusCode = 502;
        snprintf(message->response.statusMessage, sizeof(message->response.statusMessage), "%s", "Bad Gateway");
        message->response.headers.count = 0;
        message->response.body = NULL;
        message->response.bodyLength = 0;
        message->shouldCache = false;
        message->shouldReplicate = false;
        return;
    }
    HTTPResponse response = ReadHTTPResponse(backendSocket);
    DecrementActiveConnections(lb, &backend);
    CloseClientSocket(backendSocket);

    message->response = response;

    if (response.statusCode != 200)
    {
        message->shouldCache = false;
        message->shouldReplicate = false;
    }

    if (message->shouldReplicate)
    {
        ReplicatorReplicate(message->request, lb, &backend);
    }
}
