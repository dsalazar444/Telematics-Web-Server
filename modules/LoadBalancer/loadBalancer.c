#include "loadBalancer.h"

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

LoadBalancer LoadBalancerCreate(IDBackendNode *nodes, uint8_t count) {
    LoadBalancer lb;
    BackendNode *backend_nodes = malloc(sizeof(BackendNode) * count);
    for (uint8_t i = 0; i < count; i++) {
        backend_nodes[i].id = nodes[i];
        backend_nodes[i].active_connections = 0;
        backend_nodes[i].healthy = LoadBalancerHealthCheck(backend_nodes[i]);
    }
    lb.backend_nodes = backend_nodes;
    lb.backend_count = count;
    lb.rr_index = 0;
    return lb;
}

bool LoadBalancerHealthCheck(BackendNode node) {
    uint8_t ip[16] = node.id.ip;
    uint16_t port = node.id.port;
    return true;
}