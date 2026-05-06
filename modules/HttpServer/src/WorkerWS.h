// Worker.h
#ifndef WORKER_WS_H
#define WORKER_WS_H

#include "../../../Includes/ISocket.h"
typedef struct {
    IClientSocket* client;
    int logFile;
} WorkerWSArgs;

void* WorkerRun(void* arg);  

#endif