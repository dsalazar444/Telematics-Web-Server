#include "Shared/ISocket.h"
#include "modules/Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>

void* worker(void* arg) {
    IClientSocket* client = (IClientSocket*)arg;
    char buffer[4096];
    int n;
    while ((n = RecvFromClient(client, buffer, sizeof(buffer))) > 0) {
        printf("Recibidos %d bytes: %.*s\n", n, n, buffer);
        // Aquí podrías responder con SendToClient(client, ...)
    }
    printf("Conexión cerrada por el cliente o error.\n");
    CloseClientSocket(client);
    return NULL;
}

int main() {
    int port = 9090; // Puedes cambiarlo o leerlo de config después

    ISocketListener listener;
    listener.fd = CreateDualStackSocket();
    if (listener.fd < 0) return 1;

    if (BindSocket(&listener, "::", port) < 0) return 1;
    if (ListenSocket(&listener, 128) < 0) return 1;

    printf("Servidor escuchando en puerto %d\n", port);

    while (1) {
        IClientSocket* client = AcceptSocket(&listener);
        if (!client) continue;

        pthread_t tid;
        pthread_create(&tid, NULL, worker, client);
        pthread_detach(tid); // El hilo se limpia solo al terminar
    }
    return 0;
}