#include "ProxyWorker.h"
#include "../HttpParser/HttpParser.h"
#include "../HttpServer/src/Response.h"
#include "../HttpServer/src/ResponseSender.h"
#include "../Replicator/Replicator.h"
#include "../../modules/Logs/Log.h"
#include "../../Includes/HttpUtils.h"

#define LEVEL "Cache"

// Worker que maneja y gestiona la conexion al cliente, orquestando los servicios de LoadBalancer, CacheManager,
// y Replicator segun corresponda. Cada worker se ejecuta en un hilo separado.
// Pide: arg - puntero a estructura WorkerArgs 
// Retorna: NULL
void *worker(void *arg)
{
    // Extrae argumentos pasados al worker
    WorkerArgs *workerArgs = (WorkerArgs *)arg;
    IClientSocket *client = workerArgs->client;
    LoadBalancer *lb = workerArgs->lb;
    CacheManager *cacheManager = workerArgs->cacheManager;
    int logFile = workerArgs->logFile;

    char buffer[4096]; // Buffer para recibir una linea de datos del cliente
    char requestBuffer[4096]; // Buffer acumulativo para parsear el request completo
    int requestLen = 0; // Cantidad de bytes actualmente en requestBuffer

    while (1)
    {
        // Obtener datos del cliente
        memset(buffer, 0, sizeof(buffer));
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));
        if (bytes <= 0)
            break;

        if (requestLen + bytes >= sizeof(requestBuffer))
        {
            printf("Error: petición demasiado grande\n");
            break;
        }

        // Acumular datos en requestBuffer para parsear el request completo cuando este finalizado
        memcpy(requestBuffer + requestLen, buffer, bytes);
        requestLen += bytes;
        requestBuffer[requestLen] = '\0';

        // Verificar tamaños de headers y body para saber si el request completo ha sido recibido
        int headerSize = 0;
        int contentLength = 0;
        int requestInfoStatus = GetRequestSizes(requestBuffer, &headerSize, &contentLength);
        // Si aún no se han recibido todos los headers o el body, seguir esperando
        if (requestInfoStatus == 0)
            continue;

        // Hubo error al parsear headers o formato invalido
        if (requestInfoStatus < 0)
        {
            char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            SendToClient(client, bad_request, strlen(bad_request));
            break;
        }

        // Verificar que se ha recibido el request completo (headers + body)
        int expectedSize = headerSize + contentLength;
        if (requestLen < expectedSize)
            continue;

        // Parsear el request
        unsigned short statusCode = 0;
        HTTPRequest *request = ParseHTTPRequest(requestBuffer, headerSize, contentLength, &statusCode);
        
        if (request == NULL || statusCode != 0)
        {
            HTTPResponse *response = ResponseError((int)statusCode);
            if (response != NULL)
            {
                SendHTTPResponse(client, response); // Si el request no se pudo parsear correctamente, enviar error 400 al cliente
                ResponseFree(response);
            }
            else
            {
                // Si hubo un error al generar la respuesta de error, enviar un fallback genérico
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

        // Allocar ProxyMessage para empezar transmisión al servidor.
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

        // Inicializar ProxyMessage con datos del request y flags para cacheo y replicación
        memset(proxyMessage, 0, sizeof(*proxyMessage));
        proxyMessage->request = request;
        proxyMessage->shouldCache = false;
        proxyMessage->shouldReplicate = (request->method == POST);

        // Verificar si es cacheable y buscar en caché
        if (cacheManager != NULL && cacheKeyFromRequest(request, proxyMessage->cacheKey, sizeof(proxyMessage->cacheKey)))
        {
            proxyMessage->shouldCache = true;

            HTTPResponse cachedResponse = {0};
            // Intentar obtener respuesta cacheada para esta petición
            if (CacheLookUp(cacheManager, proxyMessage->cacheKey, &cachedResponse))
            {
                // HIT — enviar al cliente directo
                LogWrite(logFile,LEVEL, "Cache HIT: File served from cache" );
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

        // MISS — ir al backend
        ConnectToBackendAndForward(workerArgs, proxyMessage);

        //  Enviar respuesta al cliente
        SendHTTPResponse(client, &proxyMessage->response);

        // Invalidar caché si es POST exitoso
        if (request->method == POST && proxyMessage->response.statusCode == 200)
        {
            if (cacheManager != NULL)
            {
                CacheInvalidateByRequest(cacheManager, request);
            }
        }

        // Cachear en background si aplica
        bool bodyOwnedByCacheThread = false;
        if (proxyMessage->shouldCache && proxyMessage->response.statusCode == 200)
        {
            CacheStoreAsync(cacheManager, proxyMessage);
            bodyOwnedByCacheThread = true;
        }

        // Liberar
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

    // Cerrar conexión
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

    if (response.statusCode != 200 && response.statusCode != 201)
    {
        message->shouldCache = false;
        message->shouldReplicate = false;
    }

    if (message->shouldReplicate)
    {
        ReplicatorReplicate(message->request, lb, &backend);
    }
}
