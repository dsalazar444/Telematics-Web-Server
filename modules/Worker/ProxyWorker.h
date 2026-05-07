#ifndef PROXY_WORKER_H
#define PROXY_WORKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../../modules/Socket/Socket.h"
#include "../../modules/LoadBalancer/loadBalancer.h"
#include "../../modules/CacheManager/CacheManager.h"
#include "../HttpParser/HttpParser.h"
#include "../HttpServer/src/Response.h"
#include "../HttpServer/src/ResponseSender.h"
#include "../../Includes/messages.h"
#include "../../Includes/http.h"
#include "../../Includes/ISocket.h"

// Estructura para pasar argumentos al worker
typedef struct {
    IClientSocket *client;
    LoadBalancer *lb;  
    CacheManager *cacheManager;       
    int logFile; 
} WorkerArgs;

void* worker(void* arg);
void ConnectToBackendAndForward(WorkerArgs *w, ProxyMessage *message);
void PrintHttpRequest(const HTTPRequest *request);

#endif // PROXY_WORKER_H