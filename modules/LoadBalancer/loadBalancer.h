#include "HealthCheck.h"
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

typedef struct {
    uint8_t ip[4];
    uint16_t port;
} IDBackendNode;

typedef struct BackendNode {
    IDBackendNode id;
    uint16_t active_connections;
    bool healthy;
    uint16_t index; // Índice en el array del LoadBalancer
    uint8_t failure_count; // Para health checks
} BackendNode;

typedef struct LoadBalancer {
    BackendNode *backend_nodes;
    uint16_t backend_count;
    uint16_t rr_index;
    pthread_mutex_t lock;
} LoadBalancer;

LoadBalancer* LoadBalancerCreate(char *nodes[], uint8_t count);
IDBackendNode LoadBalancerParserNodeID(const char *node_str);
BackendNode LoadBalancerSelectBackend(LoadBalancer *lb);
void FreeLoadBalancer(LoadBalancer *lb);
void LoadBalancerPrint(LoadBalancer *lb);
void IncrementActiveConnections(LoadBalancer *lb, BackendNode *node);
void DecrementActiveConnections(LoadBalancer *lb, BackendNode *node);

#endif