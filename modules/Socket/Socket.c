#include <sys/socket.h> // socket(), bind(), listen(), accept(), setsockopt()
#include <netinet/in.h> // struct sockaddr_in6, IPPROTO_IPV6
#include <arpa/inet.h>  // inet_pton(), inet_ntop()
#include <unistd.h>     // close()

#include <stdlib.h> // malloc(), free()
#include <stdio.h>  // perror()
#include <string.h>

#include "../../Includes/ISocket.h"

int SetIPv6Only(int fd, int enable)
{
    int opt = enable;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    return 0;
}

int CreateDualStackSocket()
{
    int fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    if (SetIPv6Only(fd, 0) == -1)
    {
        close(fd);
        return -1;
    }

    return fd;
}

int BindSocket(ISocketListener *self, const char *host, int port)
{
    // Estructura de sockets que necesita POSIX para bind()
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));

    // Llena los campos necesarios que aun puede (tener en cuenta que esto solo recibe binarios, por lo que solo puede agregar port y el tipo de red)
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);

    // Es necesario convertir el host a binario para eso se usa esta funcion
    // Parametros: AF_INET6 para IPv6, host es la cadena de texto con la IP, y el ultimo parametro es un puntero a donde se va a guardar la IP en formato binario
    if (inet_pton(AF_INET6, host, &addr.sin6_addr) <= 0)
    {
        perror("inet_pton");
        return -1;
    }

    // Hace la conexion entre el socket y la direccion que se le dio, si es menor a 0 hubo un error
    // (struct sockaddr *)&addr le dice el tipo, esto debido a que no conoce sockaddr_in6, y sizeof(addr) le dice el tamaño de la estructura
    if (bind(self->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return -1;
    }

    return 0;
}

int ListenSocket(ISocketListener *self, int backlog)
{
    if (listen(self->fd, backlog) < 0)
    {
        perror("listen");
        return -1;
    }
    return 0;
}

IClientSocket *AcceptSocket(ISocketListener *self)
{
    // Crea la estructura necesaria para guardar la informacion del cliente que se va a conectar, y un entero con el tamaño de esta estructura
    struct sockaddr_in6 clientAddr;
    socklen_t addrLen = sizeof(clientAddr); // Usa este tipo de dato ya que es el estandar y adiccional evita errores

    // Acepta la conexion entrante, y guarda la informacion del cliente en clientAddr, si es menor a 0 hubo un error
    int clientFd = accept(self->fd, (struct sockaddr *)&clientAddr, &addrLen);
    if (clientFd < 0)
    {
        perror("accept");
        return NULL;
    }

    // Crea un nuevo IClientSocket para el cliente conectado
    IClientSocket *client = malloc(sizeof(IClientSocket));
    client->fd = clientFd;

    return client;
}

int RecvFromClient(IClientSocket *client, char *buf, int size)
{
    int bytesRead = recv(client->fd, buf, size, 0);
    if (bytesRead < 0)
    {
        perror("recv");
        return -1;
    }
    return bytesRead;
}

int SendToClient(IClientSocket *client, const char *data, int size)
{
    int bytesSent = send(client->fd, data, size, 0);
    if (bytesSent < 0)
    {
        perror("send");
        return -1;
    }
    return bytesSent;
}

void CloseClientSocket(IClientSocket *client)
{
    close(client->fd);
    free(client);
}
