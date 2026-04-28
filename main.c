#include "Includes/ISocket.h"
#include "modules/Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>
#include "Config/Config.h"

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

    Config config = LoadConfig("proxy.config"); 

    ISocketListener listener;
    listener.fd = CreateDualStackSocket();
    if (listener.fd < 0) return 1;

    if (BindSocket(&listener, "::", config.port) < 0) return 1;
    if (ListenSocket(&listener, 128) < 0) return 1;

    printf("Servidor escuchando en puerto %d\n", config.port);

    while (1) {
        IClientSocket* client = AcceptSocket(&listener);
    
    }
    return 0;
}