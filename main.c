#include "Includes/ISocket.h"
#include "modules/Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>
#include "Config/Config.h"
#include "modules/LoadBalancer/loadBalancer.h"
#include "modules/LoadBalancer/HealthCheck.h"
#include "modules/Worker/ProxyWorker.h"
#include "modules/CacheManager/CacheManager.h"
#include <stdlib.h>

int main()
{

    Config config = LoadConfig("../Config/proxy.config");

    ISocketListener listener;
    listener.fd = CreateDualStackSocket();
    if (listener.fd < 0)
        return 1;

    if (BindSocket(&listener, "::", config.port) < 0)
        return 1;
    if (ListenSocket(&listener, 128) < 0)
        return 1;

    printf("Servidor escuchando en puerto %d\n", config.port);


    // Crear el LoadBalancer con los backends y el contador
    LoadBalancer *lb = LoadBalancerCreate(config.backends, config.backendCount);
    LoadBalancerPrint(lb);

    // // Crear la caché
    CacheManager *cacheManager = CacheManagerCreate(config.cacheDir, config.ttl);
    if (cacheManager == NULL) {
        fprintf(stderr, "Error al crear el CacheManager\n");
        FreeLoadBalancer(lb);
        FreeConfig(&config);
        return 1;
    }

    pthread_t health_thread;
    pthread_create(&health_thread, NULL, HealthCheckLoop, lb);
    pthread_detach(health_thread); // dejar que corra independientemente
    
    while (1)
    {
        IClientSocket *client = AcceptSocket(&listener);
        if (client == NULL)
            continue;

        WorkerArgs *args = malloc(sizeof(WorkerArgs));
        args->client = client;
        args->lb = lb; // mismo puntero para todos
        args->cacheManager = cacheManager; // pasar el puntero a la caché
        pthread_t thread;
        pthread_create(&thread, NULL, worker, args);
        pthread_detach(thread);
    }
    // Cuando ya no se necesite la configuración ni el load balancer:
    FreeLoadBalancer(lb); // Si tienes una función para liberar el load balancer
    FreeConfig(&config);  // Libera la memoria de los backends
    return 0;
}