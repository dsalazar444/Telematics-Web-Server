#ifndef REPLICATOR_H
#define REPLICATOR_H

#include "messages.h"
#include "LoadBalancer/loadBalancer.h"

void ReplicatorReplicate(HTTPRequest *message, LoadBalancer *lb, BackendNode *originNode);


#endif