#include "loadBalancer.h"
#include <stdio.h>
#include <stdlib.h>

// Creación del LoadBalancer
// Pide: nodes - array de strings con IP:PUERTO de cada backend; count - cantidad de backends
// Retorna: puntero a LoadBalancer inicializado, NULL si falla malloc
LoadBalancer* LoadBalancerCreate(char *nodes[], uint8_t count) {
    LoadBalancer *lb = malloc(sizeof(LoadBalancer));
    BackendNode *backend_nodes = malloc(sizeof(BackendNode) * count);

    // Indexamos cada nodo y realizamos el health check
    for (uint8_t i = 0; i < count; i++) {
        backend_nodes[i].id = LoadBalancerParserNodeID(nodes[i]);
        backend_nodes[i].ActiveConnections = 0;
        backend_nodes[i].healthy = HealthCheckBackend(backend_nodes[i].id);
        backend_nodes[i].index = i;
        backend_nodes[i].FailureCount = 0;
    }
    lb->BackendNodes = backend_nodes;
    lb->BackendCount = count;
    lb->rrIndex = 0;

    // Inicializamos el mutex
    pthread_mutex_init(&lb->lock, NULL);

    return lb;
}

// Parsea un string con formato "IP:PUERTO" a una estructura IDBackendNode
// Pide: node_str - string con formato "IP:PUERTO"
// Retorna: IDBackendNode con campos llenados, o campos en 0 si el parseo falla
IDBackendNode LoadBalancerParserNodeID(const char *node_str) {
    IDBackendNode node = {0};
    
    if (node_str == NULL) return node;

    uint8_t a, b, c, d;
    int port;
    
    // sscanf funciona como un parser simple: intenta extraer 4 octetos y un puerto. Si no se logra, devuelve el nodo con campos en 0.
    // sscanf devuelve el número de items correctamente parseados, debe ser 5 (4 octetos + puerto)
    if (sscanf(node_str, "%hhu.%hhu.%hhu.%hhu:%d", &a, &b, &c, &d, &port) != 5) {
        return node; // Parse failed
    }
    
    if (port <= 0 || port > 65535) {
        return node;
    }

    node.ip[0] = a;
    node.ip[1] = b;
    node.ip[2] = c;
    node.ip[3] = d;
    node.port = (uint16_t)port;

    return node;
}

// Selecciona un backend saludable usando Round Robin
// Pide: lb - puntero al LoadBalancer
// Retorna: BackendNode seleccionado, o BackendNode con campos en 0 si no hay backends saludables o si lb es NULL
BackendNode LoadBalancerSelectBackend(LoadBalancer *lb){
    if (lb->BackendCount == 0) {
        BackendNode empty_node = {0};
        return empty_node;
    }

    // Bloqueamos el mutex para acceder a la lista de backends de forma segura
    pthread_mutex_lock(&lb->lock);

    // Proceso del round robin, iterando desde el índice actual y buscando un backend saludable
    for (uint16_t i = 0; i < lb->BackendCount; i++) {
        uint16_t index = (lb->rrIndex + i) % lb->BackendCount;
        if (lb->BackendNodes[index].healthy) {
            lb->rrIndex = (index + 1) % lb->BackendCount;
            BackendNode chosen = lb->BackendNodes[index];
            pthread_mutex_unlock(&lb->lock);
            return chosen;
        }
    }

    // Desbloqueamos el mutex antes de retornar si no encontramos ningún backend saludable
    pthread_mutex_unlock(&lb->lock);
    BackendNode empty_node = {0};
    return empty_node; // No healthy backends
}

// Libera la memoria asociada al LoadBalancer
// Pide: lb - puntero al LoadBalancer a liberar
// Retorna: nada
void FreeLoadBalancer(LoadBalancer *lb) {
    if (lb) {
        free(lb->BackendNodes);
        pthread_mutex_destroy(&lb->lock);
        free(lb);
    }
}

// Imprime el estado actual del LoadBalancer (para debugging)
// Pide: lb - puntero al LoadBalancer a imprimir
// Retorna: nada
void LoadBalancerPrint(LoadBalancer *lb) {
    if (!lb) {
        printf("LoadBalancer is NULL\n");
        return;
    }
    
    printf("\n=== LoadBalancer Status ===\n");
    printf("Total backends: %u\n", lb->BackendCount);
    printf("Current RR index: %u\n", lb->rrIndex);
    printf("\nBackends:\n");
    for (uint16_t i = 0; i < lb->BackendCount; i++) {
        BackendNode *bn = &lb->BackendNodes[i];
        printf("  [%u] %u.%u.%u.%u:%u | healthy=%s | active_conn=%u\n",
            bn->index,
            bn->id.ip[0], bn->id.ip[1], bn->id.ip[2], bn->id.ip[3],
            bn->id.port,
            bn->healthy ? "YES" : "NO",
            bn->ActiveConnections);
    }
    printf("============================\n\n");
}

// Incrementa el contador de conexiones activas para un backend específico
// Pide: lb - puntero al LoadBalancer; node - puntero al BackendNode para el cual incrementar el contador
// Retorna: nada
void IncrementActiveConnections(LoadBalancer *lb, BackendNode *node){
    if (!lb || !node || node->index >= lb->BackendCount) return;
    pthread_mutex_lock(&lb->lock);
    lb->BackendNodes[node->index].ActiveConnections++;
    pthread_mutex_unlock(&lb->lock);
}

// Decrementa el contador de conexiones activas para un backend específico
// Pide: lb - puntero al LoadBalancer; node - puntero al BackendNode para el cual decrementar el contador
// Retorna: nada
void DecrementActiveConnections(LoadBalancer *lb, BackendNode *node){
    if (!lb || !node || node->index >= lb->BackendCount) return;
    pthread_mutex_lock(&lb->lock);
    if (lb->BackendNodes[node->index].ActiveConnections > 0) {
        lb->BackendNodes[node->index].ActiveConnections--;
    }
    pthread_mutex_unlock(&lb->lock);
}