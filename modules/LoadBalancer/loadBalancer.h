#include "../Includes/structs.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

typedef struct BackendNode {
    IDBackendNode id;
    uint16_t active_connections;
    bool healthy;
} BackendNode;

typedef struct RequestBuffer {
    HTTPRequest requests[256];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint16_t capacity;
} RequestBuffer;

typedef struct LoadBalancer {
    BackendNode *backend_nodes;
    uint16_t backend_count;
    uint16_t rr_index;
} LoadBalancer;

void request_buffer_init(RequestBuffer *buffer, uint16_t capacity);
bool request_buffer_enqueue(RequestBuffer *buffer, const HTTPRequest *request);
bool request_buffer_dequeue(RequestBuffer *buffer, HTTPRequest *request);
uint16_t request_buffer_size(const RequestBuffer *buffer);
bool request_buffer_is_full(const RequestBuffer *buffer);

BackendNode *load_balancer_select_node(LoadBalancer *lb);
void load_balancer_mark_healthy_node(BackendNode *node);
void load_balancer_mark_unhealthy_node(BackendNode *node);
BackendNode *load_balancer_get_backend_nodes(const LoadBalancer *lb);

#endif