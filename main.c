#include "Includes/ISocket.h"
#include "modules/Socket/Socket.h"
#include <stdio.h>
#include <pthread.h>
#include "Config/Config.h"
#include "modules/LoadBalancer/loadBalancer.h"

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

    // Crear el LoadBalancer con los backends y el contador
    // LoadBalancer* lb = LoadBalancerCreate(config.backends, config.backendCount);

    while (1) {
        IClientSocket* client = AcceptSocket(&listener);
    
    }
    // Cuando ya no se necesite la configuración ni el load balancer:
    // FreeLoadBalancer(lb); // Si tienes una función para liberar el load balancer
    FreeConfig(&config); // Libera la memoria de los backends
    return 0;
}