#include "Replicator.h"
#include "../Worker/ReplicatorWorker.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// Deep-copy an HTTPRequest (caller must free copy->body and copy itself)
static HTTPRequest *CopyHTTPRequest(const HTTPRequest *src)
{
    if (src == NULL) return NULL;
    HTTPRequest *dst = malloc(sizeof(HTTPRequest));
    if (!dst) return NULL;
    // Copy fixed-size fields
    dst->method = src->method;
    strncpy(dst->path, src->path, sizeof(dst->path));
    strncpy(dst->version, src->version, sizeof(dst->version));
    // Copy headers struct
    dst->headers = src->headers;
    // Copy body if present
    dst->bodyLength = src->bodyLength;
    if (src->body != NULL && src->bodyLength > 0) {
        dst->body = malloc(src->bodyLength);
        if (!dst->body) {
            free(dst);
            return NULL;
        }
        memcpy(dst->body, src->body, src->bodyLength);
    } else {
        dst->body = NULL;
    }
    return dst;
}

void ReplicatorReplicate(const HTTPRequest *message, LoadBalancer *lb, BackendNode *originNode) {
    if (message == NULL || lb == NULL || originNode == NULL) return;

    BackendNode *nodesList = lb->backend_nodes; // direct pointer

    for (uint16_t i = 0; i < lb->backend_count; i++) {
        BackendNode *node = &nodesList[i];
        if (node->index == originNode->index) continue;
        if (!node->healthy) continue;

        // Prepare worker args with a deep copy of the request
        ReplicatorWorkerArgs *args = malloc(sizeof(ReplicatorWorkerArgs));
        if (!args) continue;
        memset(args, 0, sizeof(*args));

        HTTPRequest *req_copy = CopyHTTPRequest(message);
        if (!req_copy) {
            free(args);
            continue;
        }

        args->request = req_copy;
        args->targetNode = *node; // copy struct
        args->lb = lb;
        args->timeout_ms = 3000; // default timeout per attempt (ms)

        pthread_t tid;
        if (pthread_create(&tid, NULL, replicatorWorker, args) != 0) {
            // failed to create thread, cleanup
            free(req_copy->body);
            free(req_copy);
            free(args);
            continue;
        }
        // Detach thread; worker frees args and request
        pthread_detach(tid);
    }
}