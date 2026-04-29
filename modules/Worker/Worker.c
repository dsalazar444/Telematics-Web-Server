// Worker.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Worker.h"
#include "../Socket/Socket.h"

void *worker(void *arg)
{
    IClientSocket *client = (IClientSocket *)arg;

    // Recibir la petición
    char buffer[4096];

    while (1)
    {
        memset(buffer, 0, sizeof(buffer)); // limpia el buffer cada vez
        int bytes = RecvFromClient(client, buffer, sizeof(buffer));

        if (bytes <= 0) break;

        printf("Peticion recibida:\n%s\n", buffer);

        char *response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHola cliente";
        SendToClient(client, response, strlen(response));
    }

    // 4. Cerrar y liberar
    CloseClientSocket(client);
    free(client);
    return NULL;
}