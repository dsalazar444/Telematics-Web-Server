// Worker.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Worker.h"
#include "../Socket/Socket.h"

void *worker(void *arg)
{
    WorkerArgs *w = (WorkerArgs*)arg;

    IClientSocket *client = w->client;
    LoadBalancer *lb = w->lb;

    // Recibir la petición
    char buffer[4096];

    while (1)
    {
        memset(buffer, 0, sizeof(buffer)); // limpia el buffer cada vez
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));

        if (bytes <= 0) break;

        printf("Peticion recibida:\n%s\n", buffer);

        ConnectToBackendAndForward(w, buffer);

        char *response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHola cliente\r\n";
        SendToClient(client, response, strlen(response));
    }

    // 4. Cerrar y liberar
    CloseClientSocket(client);
    return NULL;
}

void ConnectToBackendAndForward(WorkerArgs *w, const char *request){
    // Aquí se implementaría la lógica para conectar al backend seleccionado por el LoadBalancer
    // y reenviar la petición. Esto incluiría:
    // 1. Seleccionar un backend usando LoadBalancerSelectBackend(w->lb)
    // 2. Crear un socket para conectarse al backend
    // 3. Conectar al backend usando la IP y puerto del BackendNode seleccionado
    // 4. Enviar la petición al backend
    // 5. Recibir la respuesta del backend
    // 6. Reenviar la respuesta al cliente original usando SendToClient(w->client, ...)
    LoadBalancer *lb = w->lb;
    BackendNode backend = LoadBalancerSelectBackend(lb);
    // yo  genero el proxymessage
    IncrementActiveConnections(lb, &backend);
}