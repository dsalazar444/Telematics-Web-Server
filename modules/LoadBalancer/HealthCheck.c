#include "HealthCheck.h"
#include <unistd.h>
#include <stdio.h>

#define HEALTH_CHECK_INTERVAL 5

void *HealthCheckLoop(void *arg)
{
    LoadBalancer *lb = (LoadBalancer *)arg;

    while (1)
    {
        sleep(HEALTH_CHECK_INTERVAL);

        pthread_mutex_lock(&lb->lock);
        for (uint16_t i = 0; i < lb->backend_count; i++)
        {
            BackendNode *bn = &lb->backend_nodes[i];

            // TODO: aquí haces el health check real
            // Por ahora, simulamos que siempre está sano
            bool Ishealthy = HealthCheckBackend(bn->id);

            bn->healthy = Ishealthy;
            if (Ishealthy)
            {
                bn->failure_count = 0; // reset si vuelve
            }
            else
            {
                bn->failure_count++;
            }
        }
        pthread_mutex_unlock(&lb->lock);

        printf("\033[2J\033[H");
        LoadBalancerPrint(lb); // opcional: mostrar estado
        fflush(stdout);
    }
    return NULL;
}

bool HealthCheckBackend(IDBackendNode id)
{
    // TODO: conectar a ip:port, enviar GET /health, verificar respuesta
    // Por ahora: asumimos que siempre está sano
    return true;
}