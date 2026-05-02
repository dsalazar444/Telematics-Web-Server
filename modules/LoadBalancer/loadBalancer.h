#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "HealthCheck.h"

#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

typedef struct {
    uint8_t ip[4];
    uint16_t port;
} IDBackendNode;

typedef struct BackendNode {
    IDBackendNode id;
    uint16_t ActiveConnections;
    bool healthy;
    uint16_t index; // Índice en el array del LoadBalancer
    uint8_t FailureCount; // Para health checks
} BackendNode;

typedef struct LoadBalancer {
    BackendNode *BackendNodes;
    uint16_t BackendCount;
    uint16_t rrIndex;
    pthread_mutex_t lock;
} LoadBalancer;

LoadBalancer* LoadBalancerCreate(char *nodes[], uint8_t count);
IDBackendNode LoadBalancerParserNodeID(const char *node_str);
BackendNode LoadBalancerSelectBackend(LoadBalancer *lb);
void FreeLoadBalancer(LoadBalancer *lb);
void LoadBalancerPrint(LoadBalancer *lb);
void IncrementActiveConnections(LoadBalancer *lb, BackendNode *node);
void DecrementActiveConnections(LoadBalancer *lb, BackendNode *node);

#endif // LOAD_BALANCER_H