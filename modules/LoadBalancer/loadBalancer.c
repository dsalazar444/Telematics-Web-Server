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