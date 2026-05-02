// Worker.h
#ifndef WORKER_WS_H
#define WORKER_WS_H

#include "../../../Includes/ISocket.h"
typedef struct {
    IClientSocket* client;
} WorkerWSArgs;

void* WorkerRun(void* arg);  // función del thread

#endif