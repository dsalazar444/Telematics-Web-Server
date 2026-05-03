#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include "loadBalancer.h"
#include <unistd.h>

typedef struct {
    LoadBalancer *lb;   // Compartido entre todos los workers
    int logFile; // int que da write en logInit() 
} HealthCheckArgs;

void *HealthCheckLoop(void *arg);
bool HealthCheckBackend(IDBackendNode id);

#endif // HEALTH_CHECK_H