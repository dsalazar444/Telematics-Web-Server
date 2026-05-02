#ifndef REPLICATOR_H
#define REPLICATOR_H

#include "../../Includes/http.h"
#include "../LoadBalancer/loadBalancer.h"

void ReplicatorReplicate(const HTTPRequest *message, LoadBalancer *lb, BackendNode *originNode);
static HTTPRequest *CopyHTTPRequest(const HTTPRequest *src);


#endif