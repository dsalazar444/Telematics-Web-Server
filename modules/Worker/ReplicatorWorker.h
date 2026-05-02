#ifndef REPLICATOR_WORKER_H
#define REPLICATOR_WORKER_H

#include "../Replicator/Replicator.h"
#include "../LoadBalancer/loadBalancer.h"
#include "../Socket/Socket.h"
#include <pthread.h>

typedef struct {
    HTTPRequest *request; // Mensaje a replicar (se libera al terminar)
    BackendNode targetNode; // Nodo objetivo donde replicar
    LoadBalancer *lb; // Para marcar unhealthy si falla
    int timeout_ms; // timeout por intento
} ReplicatorWorkerArgs;

void *replicatorWorker(void *arg);


#endif