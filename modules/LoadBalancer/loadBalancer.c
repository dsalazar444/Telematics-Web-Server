#include "loadBalancer.h"
#include <stdio.h>
#include <stdlib.h>

LoadBalancer* LoadBalancerCreate(char *nodes[], uint8_t count) {
    LoadBalancer *lb = malloc(sizeof(LoadBalancer));
    BackendNode *backend_nodes = malloc(sizeof(BackendNode) * count);
    for (uint8_t i = 0; i < count; i++) {
        backend_nodes[i].id = LoadBalancerParserNodeID(nodes[i]);
        backend_nodes[i].active_connections = 0;
        backend_nodes[i].healthy = HealthCheckBackend(backend_nodes[i].id);
        backend_nodes[i].index = i; // Guardamos el índice para referencia futura
    }
    lb->backend_nodes = backend_nodes;
    lb->backend_count = count;
    lb->rr_index = 0;
    pthread_mutex_init(&lb->lock, NULL);

    return lb;
}

IDBackendNode LoadBalancerParserNodeID(const char *node_str) {
    IDBackendNode node = {0};
    
    if (node_str == NULL) return node;

    uint8_t a, b, c, d;
    int port;
    
    if (sscanf(node_str, "%hhu.%hhu.%hhu.%hhu:%d", &a, &b, &c, &d, &port) != 5) {
        return node; // Parse failed
    }
    
    if (port <= 0 || port > 65535) {
        return node; // Invalid port
    }

    node.ip[0] = a;
    node.ip[1] = b;
    node.ip[2] = c;
    node.ip[3] = d;
    node.port = (uint16_t)port;

    return node;
}

BackendNode LoadBalancerSelectBackend(LoadBalancer *lb){
    if (lb->backend_count == 0) {
        BackendNode empty_node = {0};
        return empty_node;
    }

    pthread_mutex_lock(&lb->lock);

    for (uint16_t i = 0; i < lb->backend_count; i++) {
        uint16_t index = (lb->rr_index + i) % lb->backend_count;
        if (lb->backend_nodes[index].healthy) {
            lb->rr_index = (index + 1) % lb->backend_count;
            pthread_mutex_unlock(&lb->lock);
            return lb->backend_nodes[index];
        }
    }

    pthread_mutex_unlock(&lb->lock);

    BackendNode empty_node = {0};
    return empty_node; // No healthy backends
}

void FreeLoadBalancer(LoadBalancer *lb) {
    if (lb) {
        free(lb->backend_nodes);
        pthread_mutex_destroy(&lb->lock);
        free(lb);
    }
}

void LoadBalancerPrint(LoadBalancer *lb) {
    if (!lb) {
        printf("LoadBalancer is NULL\n");
        return;
    }
    
    printf("\n=== LoadBalancer Status ===\n");
    printf("Total backends: %u\n", lb->backend_count);
    printf("Current RR index: %u\n", lb->rr_index);
    printf("\nBackends:\n");
    for (uint16_t i = 0; i < lb->backend_count; i++) {
        BackendNode *bn = &lb->backend_nodes[i];
        printf("  [%u] %u.%u.%u.%u:%u | healthy=%s | active_conn=%u\n",
            bn->index,
            bn->id.ip[0], bn->id.ip[1], bn->id.ip[2], bn->id.ip[3],
            bn->id.port,
            bn->healthy ? "YES" : "NO",
            bn->active_connections);
    }
    printf("============================\n\n");
}

void IncrementActiveConnections(LoadBalancer *lb, BackendNode *node){
    if (!lb || !node || node->index >= lb->backend_count) return;
    pthread_mutex_lock(&lb->lock);
    lb->backend_nodes[node->index].active_connections++;
    pthread_mutex_unlock(&lb->lock);
}

void DecrementActiveConnections(LoadBalancer *lb, BackendNode *node){
    if (!lb || !node || node->index >= lb->backend_count) return;
    pthread_mutex_lock(&lb->lock);
    if (lb->backend_nodes[node->index].active_connections > 0) {
        lb->backend_nodes[node->index].active_connections--;
    }
    pthread_mutex_unlock(&lb->lock);
}