#ifndef REPLICATOR_WORKER_H
#define REPLICATOR_WORKER_H

#include "../Replicator/Replicator.h"

typedef struct {
    LoadBalancer *lb;   // Compartido entre todos los workers
    HTTPRequest *request // Mensaje a replicar
} WorkerArgs;

void *replicatorWorker(void *arg);


#endif