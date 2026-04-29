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

typedef struct {
    IClientSocket *client;
    LoadBalancer *lb;   // Compartido entre todos los workers
} WorkerArgs;

void* worker(void* arg);
void ConnectToBackendAndForward(WorkerArgs *w, const HTTPRequest *request);
void PrintHttpRequest(const HTTPRequest *request);
static int GetRequestSizes(const char *requestBuffer, int *headerSize, int *contentLength);

#endif