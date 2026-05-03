#include "WorkerWS.h"
#include "../../Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
// Heredados desde Worker.h (ISocket.h)

const int PORT = 8083;

int main() {
    ISocketListener listener;
    listener.fd = CreateDualStackSocket();
    if (listener.fd < 0)
        return 1;

    if (BindSocket(&listener, "::", PORT) < 0)
        return 1;
    if (ListenSocket(&listener, 128) < 0)
        return 1;

    printf("Server/1.0 HTTP escuchando en puerto %d\n", PORT);

    while (1)
    {
        IClientSocket* client = AcceptSocket(&listener);
        if (client == NULL) continue;

        WorkerWSArgs* args = malloc(sizeof(WorkerWSArgs));
        if (args == NULL) {
            CloseClientSocket(client);
            continue;
        }
        args->client = client;

        pthread_t thread;
        if (pthread_create(&thread, NULL, WorkerRun, args) != 0) {
            fprintf(stderr, "Error creando thread\n");
            free(args);
            continue;
        }
        pthread_detach(thread);
    }
    // Cuando ya no se necesite la configuración ni el load balancer:
    //FreeLoadBalancer(lb); // Si tienes una función para liberar el load balancer TODO
    return 0;
}