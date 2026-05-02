#ifndef WORKER_H
#define WORKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../../Includes/ISocket.h"
#include "../../modules/Socket/Socket.h"
#include "../../Includes/http.h"
#include "../../modules/LoadBalancer/loadBalancer.h"
#include "../../modules/CacheManager/CacheManager.h"
#include "../HttpParser/HttpParser.h"
#include "../HttpServer/src/Response.h"
#include "../HttpServer/src/ResponseSender.h"
#include "../../Includes/messages.h"

typedef struct {
    IClientSocket *client;
    LoadBalancer *lb;   // Compartido entre todos los workers
    CacheManager *cacheManager;       // Puntero a la caché compartida
} WorkerArgs;

void* worker(void* arg);
void ConnectToBackendAndForward(WorkerArgs *w, ProxyMessage *message);
void PrintHttpRequest(const HTTPRequest *request);
//int GetRequestSizes(const char *requestBuffer, int *headerSize, int *contentLength);

#endif