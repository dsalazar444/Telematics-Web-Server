#include "../../../Includes/ISocket.h"
#include "../../Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>
//#include "modules/Worker/Worker.h"
#include <stdlib.h>

const int PORT = 8082;

int main() {
    ISocketListener listener;
    listener.fd = CreateDualStackSocket();
    if (listener.fd < 0)
        return 1;

    if (BindSocket(&listener, "::", PORT) < 0)
        return 1;
    if (ListenSocket(&listener, 128) < 0)
        return 1;

    printf("Servidor HTTP escuchando en puerto %d\n", PORT);


    while (1)
    {
        IClientSocket *client = AcceptSocket(&listener);
        if (client == NULL)
            continue;

        printf("%d\n", client->fd);
    }
    //     WorkerArgs *args = malloc(sizeof(WorkerArgs)); // TODO CAMBIAR PARA MI WORKER
    //     args->client = client;

    //     pthread_t thread;
    //     pthread_create(&thread, NULL, worker, args);
    //     // TODO la logica o acciones de worker_server estan en worker.c mio
    //     pthread_detach(thread);
    // }
    // Cuando ya no se necesite la configuración ni el load balancer:
    //FreeLoadBalancer(lb); // Si tienes una función para liberar el load balancer TODO
    return 0;
}