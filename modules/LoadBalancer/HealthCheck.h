#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include "loadBalancer.h"
#include <unistd.h>

void *HealthCheckLoop(void *arg);
bool HealthCheckBackend(IDBackendNode id);

#endif // HEALTH_CHECK_H