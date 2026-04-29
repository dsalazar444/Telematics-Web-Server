#ifndef WORKER_H
#define WORKER_H

#include "../../Includes/ISocket.h"
#include "../../modules/LoadBalancer/loadBalancer.h"

typedef struct {
    IClientSocket *client;
    LoadBalancer *lb;   // Compartido entre todos los workers
} WorkerArgs;

void* worker(void* arg);
void ConnectToBackendAndForward(WorkerArgs *w, const HTTPRequest *request);

#endif