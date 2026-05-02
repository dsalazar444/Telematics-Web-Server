#include "HealthCheck.h"
#include "../Socket/Socket.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEALTH_CHECK_INTERVAL 5

static bool BackendIPToString(const IDBackendNode id, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    if (inet_ntop(AF_INET, id.ip, buffer, (socklen_t)bufferSize) == NULL)
    {
        perror("inet_ntop");
        return false;
    }

    return true;
}

void *HealthCheckLoop(void *arg)
{
    LoadBalancer *lb = (LoadBalancer *)arg;

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
        LoadBalancerPrint(lb); // opcional: mostrar estado
        fflush(stdout);
    }
    return NULL;
}

bool HealthCheckBackend(IDBackendNode id)
{
    char ipString[INET_ADDRSTRLEN];
    if (!BackendIPToString(id, ipString, sizeof(ipString)))
    {
        return false;
    }

    IClientSocket *socket = CreateClientSocket(ipString, id.port, 2000);
    if (socket == NULL)
    {
        return false;
    }

    HTTPRequest request;
    memset(&request, 0, sizeof(request));
    request.method = GET;
    snprintf(request.path, sizeof(request.path), "%s", "/logs.log");
    snprintf(request.version, sizeof(request.version), "%s", "HTTP/1.1");
    request.headers.count = 1;
    snprintf(request.headers.headers[0].key, sizeof(request.headers.headers[0].key), "%s", "Host");
    snprintf(request.headers.headers[0].value, sizeof(request.headers.headers[0].value), "%s", ipString);

    bool healthy = false;
    if (SendHTTPRequest(socket, &request) == 0)
    {
        HTTPResponse response = ReadHTTPResponse(socket);
        healthy = (response.statusCode);
        free(response.body);
    }

    CloseClientSocket(socket);
    return healthy;
}