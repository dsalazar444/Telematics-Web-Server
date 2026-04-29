#include "loadBalancer.h"
#include <stdio.h>
#include <stdlib.h>

void RequestBufferInit(RequestBuffer *buffer, uint16_t capacity) {
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->capacity = capacity;
}

bool RequestBufferEnqueue(RequestBuffer *buffer, const HTTPRequest *request) {
    if (RequestBufferIsFull(buffer)) {
        return false;
    }
    buffer->requests[buffer->tail] = *request;
    buffer->tail = RequestNextIndex(buffer->tail, buffer->capacity);
    buffer->count++;
    return true;
}

int RequestNextIndex(int current, int capacity) {
    return (current + 1) % capacity;
}

bool RequestBufferIsFull(const RequestBuffer *buffer) {
    return buffer->count == buffer->capacity;
}

bool RequestBufferDequeue(RequestBuffer *buffer, HTTPRequest *request) {
    if (buffer->count == 0) {
        return false;
    }
    *request = buffer->requests[buffer->head];
    buffer->head = RequestNextIndex(buffer->head, buffer->capacity);
    buffer->count--;
    return true;
}

LoadBalancer LoadBalancerCreate(char *nodes, uint8_t count) {
    LoadBalancer lb;
    BackendNode *backend_nodes = malloc(sizeof(BackendNode) * count);
    for (uint8_t i = 0; i < count; i++) {
        backend_nodes[i].id = LoadBalancerParserNodeID(nodes[i]);
        backend_nodes[i].active_connections = 0;
        backend_nodes[i].healthy = LoadBalancerHealthCheck(backend_nodes[i]);
    }
    lb.backend_nodes = backend_nodes;
    lb.backend_count = count;
    lb.rr_index = 0;
    return lb;
}

BackendNode LoadBalancerSelectBackend(LoadBalancer *lb){
    if (lb->backend_count == 0) {
        BackendNode empty_node = {0};
        return empty_node; // No backends available
    }

    for (uint16_t i = 0; i < lb->backend_count; i++) {
        uint16_t index = (lb->rr_index + i) % lb->backend_count;
        if (lb->backend_nodes[index].healthy) {
            lb->rr_index = (index + 1) % lb->backend_count; // Update for next round
            return lb->backend_nodes[index];
        }
    }

    BackendNode empty_node = {0};
    return empty_node; // No healthy backends found
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

bool LoadBalancerHealthCheck(BackendNode node) {
    uint8_t ip[4] = node.id.ip;
    uint16_t port = node.id.port;
    return true;
}