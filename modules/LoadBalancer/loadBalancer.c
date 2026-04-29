#include "loadBalancer.h"
//#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>


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
        backend_nodes[i].id = *LoadBalancerParserNodeID(nodes[i]);
        backend_nodes[i].active_connections = 0;
        backend_nodes[i].healthy = LoadBalancerHealthCheck(backend_nodes[i]);
    }
    lb.backend_nodes = backend_nodes;
    lb.backend_count = count;
    lb.rr_index = 0;
    return lb;
}

IDBackendNode *LoadBalancerParserNodeID(const char *node_str) {
    if (node_str == NULL) return NULL;

    const char *colon = strchr(node_str, ':');
    if (colon == NULL) return NULL;

    size_t ip_len = (size_t)(colon - node_str);
    if (ip_len == 0 || ip_len >= sizeof(((struct in6_addr *)0)->s6_addr) * 3) {
        // crude guard: ip too long
    }

    char ip_str[48];
    if (ip_len >= sizeof(ip_str)) return NULL;
    memcpy(ip_str, node_str, ip_len);
    ip_str[ip_len] = '\0';

    const char *port_str = colon + 1;
    if (*port_str == '\0') return NULL;

    char *endptr = NULL;
    long port_long = strtol(port_str, &endptr, 10);
    if (endptr == port_str || *endptr != '\0' || port_long <= 0 || port_long > 65535) return NULL;

    uint8_t ip_bytes[16] = {0};
    struct in_addr in4;
    if (inet_pton(AF_INET, ip_str, &in4) == 1) {
        memcpy(ip_bytes, &in4.s_addr, 4);
    } else {
        struct in6_addr in6;
        if (inet_pton(AF_INET6, ip_str, &in6) == 1) {
            memcpy(ip_bytes, &in6.s6_addr, 16);
        } else {
            return NULL; // invalid IP
        }
    }

    IDBackendNode *node_id = malloc(sizeof(IDBackendNode));
    if (node_id == NULL) return NULL;
    memcpy(node_id->ip, ip_bytes, sizeof(node_id->ip));
    node_id->port = (uint16_t)port_long;
    return node_id;
}

bool LoadBalancerHealthCheck(BackendNode node) {
    uint8_t ip[4] = node.id.ip;
    uint16_t port = node.id.port;
    return true;
}