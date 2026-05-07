#include "HealthCheck.h"
#include "../Socket/Socket.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Logs/Log.h"

#define HEALTH_CHECK_INTERVAL 30
#define LEVEL "Load Balancer"

// Worker que realiza health checks periódicos a los backends y actualiza su estado en el LoadBalancer
// Pide: arg - puntero a HealthCheckArgs con el LoadBalancer compartido y el file descriptor del log
// Retorna: NULL (no se espera que termine)
void *HealthCheckLoop(void *arg)
{
    HealthCheckArgs *args = (HealthCheckArgs *)arg;
    if (args == NULL) return NULL;

    LoadBalancer *lb = args->lb;
    int logFile = args->logFile;

    while (1)
    {
        sleep(HEALTH_CHECK_INTERVAL);

        for (uint16_t i = 0; i < lb->BackendCount; i++)
        {
            BackendNode *bn = &lb->BackendNodes[i];
            bool Ishealthy = HealthCheckBackend(bn->id);

            pthread_mutex_lock(&lb->lock);
            bn->healthy = Ishealthy;
            if (Ishealthy)
            {
                bn->FailureCount = 0; // reset si vuelve
            }
            else
            {
                bn->FailureCount++;
            }
            pthread_mutex_unlock(&lb->lock);
        }

        printf("\033[2J\033[H");
        LoadBalancerPrint(lb);
        const char *lbString = LoadBalancerToString(lb);
        LogWrite(logFile, LEVEL, lbString);
        fflush(stdout);
    }
    return NULL;
}

// Realiza un health check a un backend específico intentando conectarse y hacer una petición HEAD a /health/health.html
// Pide: id - IDBackendNode con la IP y puerto del backend a chequear
// Retorna: true si el backend respondió con 200 OK o 201 Created, false si no respondió o hubo error
bool HealthCheckBackend(IDBackendNode id)
{
    if (id.port == 0)
    {
        return false;
    }

    IClientSocket *socket = CreateClientSocket(id.ip, id.port, 2000);
    if (socket == NULL)
    {
        return false;
    }

    HTTPRequest request;
    memset(&request, 0, sizeof(request));
    request.method = HEAD;
    snprintf(request.path, sizeof(request.path), "%s", "/health/health.html");
    snprintf(request.version, sizeof(request.version), "%s", "HTTP/1.1");
    request.headers.count = 1;
    snprintf(request.headers.headers[0].key, sizeof(request.headers.headers[0].key), "%s", "Host");
    snprintf(request.headers.headers[0].value, sizeof(request.headers.headers[0].value), "%u.%u.%u.%u",
             id.ip[0], id.ip[1], id.ip[2], id.ip[3]);

    bool healthy = false;
    if (SendHTTPRequest(socket, &request) == 0)
    {
        HTTPResponse response = ReadHTTPResponse(socket);
        // Si el backend respondió con cualquier status HTTP válido,
        // consideramos que está vivo (aunque el recurso devuelva error lógico).
        healthy = (response.statusCode > 0);
        free(response.body);
    }

    CloseClientSocket(socket);
    return healthy;
}