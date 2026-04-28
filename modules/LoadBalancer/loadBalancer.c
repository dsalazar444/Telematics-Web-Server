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

LoadBalancer loadBalancerCreate(IDBackendNode *nodes, uint16_t count) {
    LoadBalancer lb;
    for (uint8_t i = 0; i < count; i++) {
        lb.backend_nodes[i].id = nodes[i];
        lb.backend_nodes[i].active_connections = 0;
        lb.backend_nodes[i].healthy = true;
    }
    lb.backend_nodes = nodes;
    lb.backend_count = count;
    lb.rr_index = 0;
    return lb;
}

bool LoadBalancerCheckHealth(BackendNode node) {
    IDBackendNode id = node.id;
    return true; // Placeholder: Assume all nodes are healthy
}