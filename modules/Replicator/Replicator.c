#include "Replicator.h"

void ReplicatorReplicate(HTTPRequest *message, LoadBalancer *lb, BackendNode *originNode) {
    BackendNode *nodesList = LoadBalancerGetBackendNodes(lb);

    for (uint16_t i = 0; i < lb->backend_count; i++) {
        BackendNode *node = &nodesList[i];
        if (node->index != originNode->index && node->healthy) {
            // Aquí iría la lógica real de replicación, por ahora solo imprimimos
            printf("Replicando a %d.%d.%d.%d:%d\n",
                node->id.ip[0], node->id.ip[1], node->id.ip[2], node->id.ip[3], node->id.port);
        }
    }
}